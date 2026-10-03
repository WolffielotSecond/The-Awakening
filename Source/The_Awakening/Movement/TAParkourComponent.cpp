#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "Movement/TAMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/TAFreezeComponent.h"
#include "The_AwakeningPlayerController.h"

UTAParkourComponent::UTAParkourComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTAParkourComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
}

void UTAParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsParkouring || !OwnerCharacter.IsValid())
	{
		return;
	}

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
	}

	ParkourTime += DeltaTime;
	OwnerCharacter->SetActorRotation(ParkourFacingRotation);
	const float Alpha = FMath::Clamp(ParkourTime / ParkourDuration, 0.f, 1.f);

	const FVector NewLoc = EvalParabola(ParkourStart, ParkourEnd, ParkourArcHeight, Alpha);
	OwnerCharacter->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		FinishParkour();
		return;
	}

	if (Alpha > 0.85f && HasLanded())
	{
		FinishParkour();
	}
}

bool UTAParkourComponent::CanParkourToMarker(ATAParkourMarker* Marker) const
{
	if (bIsParkouring || !IsValid(Marker) || !Marker->IsValidMarker() || !OwnerCharacter.IsValid() ||
		!OverlappingMarkers.Contains(Marker) || ConsumedHeldMarkers.Contains(Marker) ||
		UTAFreezeComponent::IsActorFrozen(OwnerCharacter.Get()))
	{
		return false;
	}
	if ((OwnerCharacter->GetController() && OwnerCharacter->GetController()->IsMoveInputIgnored()) ||
		!OwnerCharacter->GetCharacterMovement() || OwnerCharacter->GetCharacterMovement()->MovementMode == MOVE_None) return false;

	const FVector Direction = (Marker->GetLandingLocation(OwnerCharacter->GetActorLocation()) -
		OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	// Pure vertical movement has no horizontal facing requirement.
	if (Direction.IsNearlyZero()) return true;
	// 可随时关闭的设计例外：静止时允许背对落点，启动后再强制转向。
	if (bAllowStationaryBackFacing && OwnerCharacter->GetVelocity().SizeSquared2D() <=
		FMath::Square(FMath::Max(0.f, StationarySpeedThreshold))) return true;
	return FVector::DotProduct(OwnerCharacter->GetActorForwardVector().GetSafeNormal2D(), Direction) >= -KINDA_SMALL_NUMBER;
}

bool UTAParkourComponent::StartParkour(ATAParkourMarker* Marker)
{
	if (!CanParkourToMarker(Marker)) return false;

	ParkourStart = OwnerCharacter->GetActorLocation();
	PreParkourVelocity = OwnerCharacter->GetVelocity();
	PreParkourVelocity.Z = 0.f;
	if (auto* Move = Cast<UTAMovementComponent>(OwnerCharacter->GetCharacterMovement())) Move->CancelParkourLanding();
	//ParkourEnd = Marker->GetLandingLocation();
	ParkourEnd = Marker->GetLandingLocation(ParkourStart);
	const FVector Direction = (ParkourEnd - ParkourStart).GetSafeNormal2D();
	ParkourFacingRotation = Direction.IsNearlyZero() ? OwnerCharacter->GetActorRotation() : Direction.Rotation();
	bParkourFacingRight = FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER ? Direction.Y > 0.f :
		OwnerCharacter->GetActorForwardVector().Y >= 0.f;
	bSavedUseControllerRotationYaw = OwnerCharacter->bUseControllerRotationYaw;
	OwnerCharacter->bUseControllerRotationYaw = false;
	OwnerCharacter->SetActorRotation(ParkourFacingRotation);

	ParkourArcHeight = Marker->ArcHeight;
	ParkourDuration = FMath::Max(Marker->JumpDuration, 0.05f);
	ParkourTime = 0.f;
	bIsParkouring = true;
	ConsumedHeldMarkers.Add(Marker);

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		OwnerCharacter->ConsumeMovementInputVector();
		bSavedOrientRotationToMovement = Move->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = Move->bUseControllerDesiredRotation;
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
		Move->SetMovementMode(MOVE_Flying);
	}

	return true;
}

void UTAParkourComponent::FinishParkour()
{
	if (!bIsParkouring)
	{
		return;
	}

	bIsParkouring = false;
	// Leaving during the jump must not rearm a marker if we land back inside it.
	for (auto It = ConsumedHeldMarkers.CreateIterator(); It; ++It)
		if (!It->IsValid() || !OverlappingMarkers.Contains(It->Get())) It.RemoveCurrent();

	if (!OwnerCharacter.IsValid())
	{
		return;
	}

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Move->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		Move->SetMovementMode(MOVE_Walking);
		// Preserve the actual takeoff momentum; releasing/changing input in flight only changes the landing target velocity.
		if (auto* LandingMove = Cast<UTAMovementComponent>(Move)) LandingMove->BeginParkourLanding(PreParkourVelocity);
		else Move->Velocity = PreParkourVelocity;
	}
	OwnerCharacter->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
}

bool UTAParkourComponent::HasLanded() const
{
	if (!OwnerCharacter.IsValid())
	{
		return false;
	}

	if (ParkourTime < 0.08f)
	{
		return false;
	}

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move)
	{
		return false;
	}

	FFindFloorResult FloorResult;
	Move->FindFloor(OwnerCharacter->GetActorLocation(), FloorResult, false);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return FloorResult.FloorDist <= Move->MAX_FLOOR_DIST + 5.f;
}

FVector UTAParkourComponent::EvalParabola(const FVector& Start, const FVector& End, float ArcHeight, float Alpha)
{
	const FVector Linear = FMath::Lerp(Start, End, Alpha);
	const float HeightOffset = 4.f * ArcHeight * Alpha * (1.f - Alpha); // 顶点在 Alpha=0.5
	return Linear + FVector(0.f, 0.f, HeightOffset);
}

ATAParkourMarker* UTAParkourComponent::FindCurrentMarkerOfType(ETAParkourMarkerType Type) const
{
	for (int32 i = OverlappingMarkers.Num() - 1; i >= 0; --i)
	{
		ATAParkourMarker* Marker = OverlappingMarkers[i];
		if (IsValid(Marker) && Marker->MarkerType == Type && CanPlayerParkourToMarker(Marker))
		{
			return Marker;
		}
	}
	return nullptr;
}

void UTAParkourComponent::TryParkourJump()
{
	if (bIsParkouring)
	{
		return;
	}

	if (ATAParkourMarker* Marker = FindCurrentMarkerOfType(ETAParkourMarkerType::JumpToPoint))
	{
		StartParkour(Marker);
	}
}

void UTAParkourComponent::TryParkourDrop()
{
	if (bIsParkouring)
	{
		return;
	}

	if (ATAParkourMarker* Marker = FindCurrentMarkerOfType(ETAParkourMarkerType::DropDown))
	{
		StartParkour(Marker);
	}
}

void UTAParkourComponent::RegisterMarker(ATAParkourMarker* Marker)
{
	if (!Marker)
	{
		return;
	}

	OverlappingMarkers.Remove(Marker);
	OverlappingMarkers.Add(Marker);
}

void UTAParkourComponent::UnregisterMarker(ATAParkourMarker* Marker)
{
	OverlappingMarkers.Remove(Marker);
	if (!bIsParkouring) ConsumedHeldMarkers.Remove(Marker);
}

void UTAParkourComponent::UpdateHeldRequests(bool bJumpHeld, bool bDropHeld)
{
	for (auto It = ConsumedHeldMarkers.CreateIterator(); It; ++It)
	{
		const ATAParkourMarker* Marker = It->Get();
		if (!Marker || (Marker->MarkerType == ETAParkourMarkerType::JumpToPoint ? !bJumpHeld : !bDropHeld))
			It.RemoveCurrent();
	}
	// Stable priority when both are held: jump first; drop may run if no valid jump starts.
	if (bJumpHeld) TryParkourJump();
	if (bDropHeld && !bIsParkouring) TryParkourDrop();
}

bool UTAParkourComponent::GetParkourFacing() const
{
	// Fixed at launch; reaching/passing the landing point cannot flip the animation.
	return bParkourFacingRight;
}

bool UTAParkourComponent::CanPlayerParkourToMarker(ATAParkourMarker* Marker) const
{
	const auto* PC = OwnerCharacter.IsValid() ? Cast<AThe_AwakeningPlayerController>(OwnerCharacter->GetController()) : nullptr;
	return (!PC || PC->AllowsInput(ETAInputCapability::Parkour)) && CanParkourToMarker(Marker);
}
