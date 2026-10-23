#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Scan/TAScanTypes.h"
#include "Health/TADamageTypes.h"
#include "The_AwakeningCharacter.h"
#include "TAHealthTestListener.generated.h"

/** Holds both presentation phases open to verify asynchronous completion and run isolation. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class ATADeathPresentationTestCharacter : public AThe_AwakeningCharacter
{
	GENERATED_BODY()
public:
	int32 DeathRequests = 0;
	int32 BlackoutRequests = 0;
	int32 LastRunId = 0;
	FGameplayTag LastSource;
	ETADamageType LastType = ETADamageType::Normal;
	bool bWasInputLocked = false;
	virtual void PlayDeathPresentation_Implementation(UTARespawnSubsystem* Respawn, int32 RunId, FGameplayTag Source, ETADamageType Type) override;
	virtual void PlayBlackoutPresentation_Implementation(UTARespawnSubsystem* Respawn, int32 RunId, FGameplayTag Source, ETADamageType Type) override
	{ ++BlackoutRequests; LastRunId = RunId; }
};

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
