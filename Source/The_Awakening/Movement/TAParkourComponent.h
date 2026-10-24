#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "Movement/TAParkourMarker.h"
#include "TAParkourComponent.generated.h"
class UInputAction;
class AThe_AwakeningPlayerController;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAParkourComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class FTAParkourPhysicalReleaseTest;

public:
	UTAParkourComponent();
	void ConfigureExecutionActions(UInputAction* Upper, UInputAction* Lower);
	/** Shared, input-independent winner for execution and both landing prompts. */
	ATAParkourMarker* SelectParkourMarker() const;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const TArray<TObjectPtr<ATAParkourMarker>>& GetOverlappingMarkers() const { return OverlappingMarkers; }

	/** Submit a physically held Upper action through the shared player input path. */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void TryParkourJump();

	/** Submit a physically held Lower action through the shared player input path. */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void TryParkourDrop();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool IsParkouring() const { return bIsParkouring; }

	/**
	 * 立即中止跑酷（死亡等外部打断）：恢复转向设置，丢弃起跳惯性，切到 Falling 靠重力落地。
	 * 与 FinishParkour 不同，不触发落地惯性；已消耗的按住标记保持不变，松开后照常重新武装。
	 */
	void AbortParkour();

	/** Shared by execution and landing previews. Includes overlap and held-marker rearm rules. */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool CanParkourToMarker(ATAParkourMarker* Marker) const;
	/** Includes the strict horizontal facing cone for this logical landing. */
	bool CanParkourToLanding(ATAParkourMarker* Marker, bool bLower) const;
	bool CanPlayerParkourToMarker(ATAParkourMarker* Marker) const;

	/** Compatibility only: serialized by BP_ThirdPersonCharacter. Never bypasses the strict facing cone. */
	UPROPERTY(BlueprintReadWrite, Category = "Parkour|Legacy")
	bool bAllowStationaryBackFacing = true;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool GetParkourFacing() const;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	float GetParkourDuration() const { return ParkourDuration; }

	void RegisterMarker(ATAParkourMarker* Marker);
	void UnregisterMarker(ATAParkourMarker* Marker);
	/** Rearm from physical source observations even when starting is unauthorized. */
	void UpdatePlayerHeldRequests(AThe_AwakeningPlayerController* PC, const UInputAction* JumpAction, const UInputAction* DropAction);

protected:
	bool IsFacingLanding(ATAParkourMarker* Marker, bool bLower) const;
	bool StartParkour(ATAParkourMarker* Marker, const TArray<FKey>& ConsumptionSources, bool bLower);
	void FinishParkour();
	bool HasLanded() const;

	/** 抛物线：t∈[0,1] */
	static FVector EvalParabola(const FVector& Start, const FVector& End, float ArcHeight, float Alpha);

protected:
	UPROPERTY(Transient) TObjectPtr<UInputAction> UpperExecutionAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LowerExecutionAction;

	UPROPERTY()
	TArray<TObjectPtr<ATAParkourMarker>> OverlappingMarkers;

	bool bIsParkouring = false;
	float ParkourTime = 0.f;
	float ParkourDuration = 0.5f;
	float ParkourArcHeight = 120.f;
	FVector ParkourStart = FVector::ZeroVector;
	FVector PreParkourVelocity = FVector::ZeroVector;
	FVector ParkourEnd = FVector::ZeroVector;
	FRotator ParkourFacingRotation = FRotator::ZeroRotator;
	bool bParkourFacingRight = true;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	bool bSavedUseControllerRotationYaw = false;

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	// Key identities participating at consumption, not cached values or observations.
	// Consumption belongs to the current overlap; real exit or source release rearms.
	TMap<TWeakObjectPtr<ATAParkourMarker>, TArray<FKey>> ConsumedHeldMarkers;
};
