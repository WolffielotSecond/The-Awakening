#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TAFreezeSubsystem.generated.h"

class UTAFreezeComponent;

/** Local gameplay freeze registry. World time and rendering remain untouched. */
UCLASS()
class THE_AWAKENING_API UTAFreezeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterParticipant(UTAFreezeComponent* Component);
	void UnregisterParticipant(UTAFreezeComponent* Component);
	void RequestFreeze(UObject* Source, float Strength = 1.f);
	void ReleaseFreeze(UObject* Source);
	bool IsFrozen() const { return GetFreezeStrength() >= 1.f; }
	float GetFreezeStrength() const;

private:
	void ApplyState();
	TSet<TWeakObjectPtr<UTAFreezeComponent>> Participants;
	TMap<TWeakObjectPtr<UObject>, float> Sources;
};
