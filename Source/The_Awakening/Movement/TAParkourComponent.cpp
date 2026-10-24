#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "Movement/TAMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Core/TAFreezeComponent.h"
#include "The_AwakeningPlayerController.h"
#include "InputActionValue.h"

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
		!Marker->TriggerBox || !Marker->TriggerBox->IsOverlappingActor(OwnerCharacter.Get()) ||
		UTAFreezeComponent::IsActorFrozen(OwnerCharacter.Get()))
	{
		return false;
	}
	if ((OwnerCharacter->GetController() && OwnerCharacter->GetController()->IsMoveInputIgnored()) ||
		!OwnerCharacter->GetCharacterMovement() || OwnerCharacter->GetCharacterMovement()->MovementMode == MOVE_None) return false;

	return IsFacingLanding(Marker, false) || IsFacingLanding(Marker, true);
}

bool UTAParkourComponent::IsFacingLanding(ATAParkourMarker* Marker, bool bLower) const
{
	if (!OwnerCharacter.IsValid() || !IsValid(Marker) || !Marker->HasValidLanding(bLower)) return false;
	const FVector Direction = (Marker->GetLandingLocationForSlot(OwnerCharacter->GetActorLocation(), bLower) -
		OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	// A purely vertical route has no horizontal bearing; keep the existing vertical behavior.
	if (Direction.IsNearlyZero()) return true;
	const FVector Facing = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	const double MinimumDot = FMath::Cos(FMath::DegreesToRadians(80.0));
	// Exclude both 80-degree edges, including floating-point error at the boundary.
	return FVector::DotProduct(Facing, Direction) > MinimumDot + 1.e-6;
}

bool UTAParkourComponent::CanParkourToLanding(ATAParkourMarker* Marker, bool bLower) const
{
	return CanParkourToMarker(Marker) && IsFacingLanding(Marker, bLower);
}

bool UTAParkourComponent::StartParkour(ATAParkourMarker* Marker, const TArray<FKey>& ConsumptionSources, bool bLower)
{
	if (!CanPlayerParkourToMarker(Marker) || !CanParkourToLanding(Marker, bLower)) return false;

	ParkourStart = OwnerCharacter->GetActorLocation();
	PreParkourVelocity = OwnerCharacter->GetVelocity();
	PreParkourVelocity.Z = 0.f;
	if (auto* Move = Cast<UTAMovementComponent>(OwnerCharacter->GetCharacterMovement())) Move->CancelParkourLanding();
	ParkourEnd = Marker->GetLandingLocationForSlot(ParkourStart, bLower);
	const FVector Direction = (ParkourEnd - ParkourStart).GetSafeNormal2D();
	ParkourFacingRotation = Direction.IsNearlyZero() ? OwnerCharacter->GetActorRotation() : Direction.Rotation();
	bParkourFacingRight = FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER ? Direction.Y > 0.f :
		OwnerCharacter->GetActorForwardVector().Y >= 0.f;
	bSavedUseControllerRotationYaw = OwnerCharacter->bUseControllerRotationYaw;
	OwnerCharacter->bUseControllerRotationYaw = false;
	OwnerCharacter->SetActorRotation(ParkourFacingRotation);

	ParkourArcHeight = Marker->GetArcHeight(bLower);
	ParkourDuration = FMath::Max(Marker->GetJumpDuration(bLower), 0.05f);
	ParkourTime = 0.f;
	bIsParkouring = true;
	ConsumedHeldMarkers.Add(Marker, ConsumptionSources);

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
	// Real exits already end their overlap opportunity. Finish only prunes dead markers.
	for (auto It = ConsumedHeldMarkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

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

void UTAParkourComponent::AbortParkour()
{
	if (!bIsParkouring)
	{
		return;
	}

	bIsParkouring = false;
	for (auto It = ConsumedHeldMarkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

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
		// 空中中止：交给重力落地，落地后 CharacterMovement 自动切回 Walking
		Move->SetMovementMode(MOVE_Falling);
		if (auto* LandingMove = Cast<UTAMovementComponent>(Move)) LandingMove->CancelParkourLanding();
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

void UTAParkourComponent::ConfigureExecutionActions(UInputAction* Upper, UInputAction* Lower)
{
	UpperExecutionAction = Upper;
	LowerExecutionAction = Lower;
}
ATAParkourMarker* UTAParkourComponent::SelectParkourMarker() const
{
	ATAParkourMarker* Best = nullptr;
	float BestDistance = MAX_flt;
	for (ATAParkourMarker* Marker : OverlappingMarkers)
	{
		if (!CanPlayerParkourToMarker(Marker)) continue;
		const float Distance = FVector::DistSquared(OwnerCharacter->GetActorLocation(), Marker->GetActorLocation());
		// Exact distance ties use stable object path; registration order never changes the winner.
		if (!Best || Distance < BestDistance || (Distance == BestDistance && Marker->GetPathName() < Best->GetPathName()))
		{ Best = Marker; BestDistance = Distance; }
	}
	return Best;
}
void UTAParkourComponent::TryParkourJump()
{
	auto* PC = OwnerCharacter.IsValid() ? Cast<AThe_AwakeningPlayerController>(OwnerCharacter->GetController()) : nullptr;
	if (PC && PC->ReadHeldAction(UpperExecutionAction).IsNonZero())
		UpdatePlayerHeldRequests(PC, UpperExecutionAction, LowerExecutionAction);
}

void UTAParkourComponent::TryParkourDrop()
{
	auto* PC = OwnerCharacter.IsValid() ? Cast<AThe_AwakeningPlayerController>(OwnerCharacter->GetController()) : nullptr;
	if (PC && PC->ReadHeldAction(LowerExecutionAction).IsNonZero())
		UpdatePlayerHeldRequests(PC, UpperExecutionAction, LowerExecutionAction);
}

void UTAParkourComponent::RegisterMarker(ATAParkourMarker* Marker)
{
	if (!Marker)
	{
		return;
	}

	OverlappingMarkers.Remove(Marker);
	OverlappingMarkers.Add(Marker);
	// Repeated registration is not a new entry and must not rearm consumption.
}

void UTAParkourComponent::UnregisterMarker(ATAParkourMarker* Marker)
{
	// The marker callback verifies the actor's final component has exited.
	// Exit ends this overlap opportunity even during an active parkour.
	if (OverlappingMarkers.Remove(Marker) > 0) ConsumedHeldMarkers.Remove(Marker);
}

void UTAParkourComponent::UpdatePlayerHeldRequests(AThe_AwakeningPlayerController* PC,
	const UInputAction* JumpAction, const UInputAction* DropAction)
{
	if (!PC || !PC->IsLocalController() || !OwnerCharacter.IsValid() || OwnerCharacter->GetController() != PC) return;
	for (auto It = ConsumedHeldMarkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent(); continue;
		}
		bool bAllSourcesReleased = !It.Value().IsEmpty();
		for (const FKey Key : It.Value())
		{
			const auto Observation = PC->FindHeldKeyObservation(Key);
			if (!Observation.IsSet() || !Observation.GetValue().IsNearlyZero())
			{ bAllSourcesReleased = false; break; }
		}
		if (bAllSourcesReleased)
		{
			It.RemoveCurrent();
		}
	}
	if (bIsParkouring || !PC->AllowsInput(ETAInputCapability::Parkour)) return;
	const bool bUpperHeld = PC->ReadHeldAction(JumpAction).IsNonZero();
	const bool bLowerHeld = PC->ReadHeldAction(DropAction).IsNonZero();
	if (bUpperHeld == bLowerHeld) return; // No request, or conflicting Upper/Lower requests.
	ATAParkourMarker* Marker = SelectParkourMarker();
	if (!Marker) return;
	// Action and logical slot are strictly paired, including on single-landing markers.
	if (!CanParkourToLanding(Marker, bLowerHeld)) return;
	const TArray<FKey> Sources = PC->GetObservedHeldKeysForAction(bLowerHeld ? DropAction : JumpAction);
	if (!Sources.IsEmpty()) StartParkour(Marker, Sources, bLowerHeld);
}

bool UTAParkourComponent::GetParkourFacing() const
{
	// Fixed at launch; reaching/passing the landing point cannot flip the animation.
	return bParkourFacingRight;
}

bool UTAParkourComponent::CanPlayerParkourToMarker(ATAParkourMarker* Marker) const
{
	const auto* PC = OwnerCharacter.IsValid() ? Cast<AThe_AwakeningPlayerController>(OwnerCharacter->GetController()) : nullptr;
	return (!PC || (PC->AllowsInput(ETAInputCapability::Parkour) && PC->AllowsInput(ETAInputCapability::Move))) && CanParkourToMarker(Marker);
}
