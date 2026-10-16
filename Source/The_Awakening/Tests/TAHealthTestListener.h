#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Scan/TAScanTypes.h"
#include "Health/TADamageTypes.h"
#include "TAHealthTestListener.generated.h"

/** 仅供自动化测试：记录生命组件动态委托的广播。 */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class THE_AWAKENING_API UTAHealthTestListener : public UObject
{
	GENERATED_BODY()

public:
	struct FHealthEvent { float Current; float Max; float Delta; };
	TArray<FHealthEvent> HealthEvents;
	int32 DeathStartedCount = 0;
	int32 RespawnedCount = 0;

	UFUNCTION()
	void HandleHealthChanged(float Current, float Max, float Delta) { HealthEvents.Add({Current, Max, Delta}); }
	UFUNCTION()
	void HandleDeathStarted() { ++DeathStartedCount; }
	UFUNCTION()
	void HandleRespawned() { ++RespawnedCount; }

	int32 WorldResetCount = 0;
	UFUNCTION()
	void HandleWorldReset() { ++WorldResetCount; }

	int32 ModifierExpiredCount = 0;
	UFUNCTION()
	void HandleModifierExpired(FTADamageModifierHandle Handle) { ++ModifierExpiredCount; }

	TArray<ETAScanEndReason> ScanEndReasons;
	UFUNCTION()
	void HandleScanEnded(ETAScanEndReason Reason) { ScanEndReasons.Add(Reason); }
};
