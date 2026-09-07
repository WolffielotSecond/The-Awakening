#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

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

bool UTAParkourComponent::StartParkour(ATAParkourMarker* Marker)
{
	if (!Marker || !Marker->IsValidMarker() || !OwnerCharacter.IsValid())
	{
		return false;
	}

	ParkourStart = OwnerCharacter->GetActorLocation();
	//ParkourEnd = Marker->GetLandingLocation();
	ParkourEnd = Marker->GetLandingLocation(ParkourStart);

	ParkourArcHeight = Marker->ArcHeight;
	ParkourDuration = FMath::Max(Marker->JumpDuration, 0.05f);
	ParkourTime = 0.f;
	bIsParkouring = true;

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
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

	if (!OwnerCharacter.IsValid())
	{
		return;
	}

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->SetMovementMode(MOVE_Walking);
	}
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
		if (Marker && Marker->MarkerType == Type)
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
}

bool UTAParkourComponent::GetParkourFacing() const
{
	if (!OwnerCharacter.IsValid())
	{
		return true;
	}

	const FVector PlayerLocation = OwnerCharacter->GetActorLocation();

	return ParkourEnd.Y >= PlayerLocation.Y;
}