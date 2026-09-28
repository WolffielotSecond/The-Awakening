#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TAMovementComponent.generated.h"

/** Normal character movement with a brief, tunable parkour landing response. */
UCLASS()
class THE_AWAKENING_API UTAMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parkour|Landing", meta=(ClampMin="0"))
	float LandingResponseDuration = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parkour|Landing", meta=(ClampMin="0"))
	float LandingBrakingDeceleration = 2500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parkour|Landing", meta=(ClampMin="0"))
	float LandingAcceleration = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Parkour|Landing", meta=(ClampMin="0"))
	float LandingFriction = 2.f;

	void BeginParkourLanding(const FVector& SavedVelocity);
	void CancelParkourLanding() { LandingTimeRemaining = 0.f; }
	bool IsParkourLanding() const { return LandingTimeRemaining > 0.f; }
	virtual float GetMaxAcceleration() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void PhysicsRotation(float DeltaTime) override;
private:
	float LandingTimeRemaining = 0.f;
};
