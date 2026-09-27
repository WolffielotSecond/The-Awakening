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
	void RequestFreeze(UObject* Source);
	void ReleaseFreeze(UObject* Source);
	bool IsFrozen() const { return !Sources.IsEmpty(); }

private:
	void ApplyState();
	TSet<TWeakObjectPtr<UTAFreezeComponent>> Participants;
	TSet<TWeakObjectPtr<UObject>> Sources;
};
