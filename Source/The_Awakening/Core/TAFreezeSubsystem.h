#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TAFreezeSubsystem.generated.h"

class UTAFreezeComponent;

/**
 * Local gameplay freeze registry. World time and rendering remain untouched.
 * 功能代码不要直接调用本类的 RequestFreeze / ReleaseFreeze，统一走 UTATimeSubsystem；
 * 这里保留为实体冻结的执行层（参与者注册、强度应用），供 UTATimeSubsystem 和冻结测试使用。
 */
UCLASS()
class THE_AWAKENING_API UTAFreezeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterParticipant(UTAFreezeComponent* Component);
	void UnregisterParticipant(UTAFreezeComponent* Component);
	/** Establish/update a live request, including strength zero. Only Release ends it. */
	void RequestFreeze(UObject* Source, float Strength = 1.f);
	void ReleaseFreeze(UObject* Source);
	bool HasFreezeRequestFor(const UTAFreezeComponent* Participant) const;
	bool IsFrozen() const { return GetFreezeStrength() >= 1.f; }
	float GetFreezeStrength() const;

private:
	void ApplyState();
	TSet<TWeakObjectPtr<UTAFreezeComponent>> Participants;
	TMap<TWeakObjectPtr<UObject>, float> Sources;
};
