#include "Movement/TAMovementComponent.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"

void UTAMovementComponent::BeginParkourLanding(const FVector& SavedVelocity)
{
	Velocity = FVector(SavedVelocity.X, SavedVelocity.Y, 0.f);
	LandingTimeRemaining = FMath::Max(0.f, LandingResponseDuration);
}

float UTAMovementComponent::GetMaxAcceleration() const
{
	return IsParkourLanding() ? FMath::Max(0.f, LandingAcceleration) : Super::GetMaxAcceleration();
}

void UTAMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (!IsParkourLanding() || !IsMovingOnGround())
	{
		if (!IsMovingOnGround()) CancelParkourLanding();
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}
	const auto* PC = CharacterOwner ? Cast<AThe_AwakeningPlayerController>(CharacterOwner->GetController()) : nullptr;
	if (PC && PC->IsMoveInputIgnored())
	{
		StopMovementImmediately();
		CancelParkourLanding();
		return;
	}
	// Scoped overrides preserve normal walking settings, including Blueprint overrides.
	const bool bSavedSeparateBraking = bUseSeparateBrakingFriction;
	bUseSeparateBrakingFriction = false;
	Super::CalcVelocity(DeltaTime, FMath::Max(0.f, LandingFriction), bFluid,
		FMath::Max(0.f, LandingBrakingDeceleration));
	bUseSeparateBrakingFriction = bSavedSeparateBraking;
	// Input filtering alone cannot stop inherited momentum from crossing an unsafe ledge.
	if (const auto* Character = Cast<AThe_AwakeningCharacter>(CharacterOwner);
		Character && !Character->IsLandingMomentumSafe(Velocity))
	{
		Velocity.X = Velocity.Y = 0.f;
	}
	LandingTimeRemaining = FMath::Max(0.f, LandingTimeRemaining - DeltaTime);
}

void UTAMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (IsParkourLanding() && Velocity.SizeSquared2D() > FMath::Square(5.f) && UpdatedComponent)
	{
		MoveUpdatedComponent(FVector::ZeroVector, Velocity.GetSafeNormal2D().Rotation(), false);
		return;
	}
	Super::PhysicsRotation(DeltaTime);
}
