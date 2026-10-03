#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Movement/TAParkourMarker.h"
#include "TAParkourComponent.generated.h"
class UInputAction;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAParkourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTAParkourComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const TArray<TObjectPtr<ATAParkourMarker>>& GetOverlappingMarkers() const { return OverlappingMarkers; }

	/** 尝试跨台跳（空格） */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void TryParkourJump();

	/** 尝试高台跳下（Ctrl） */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void TryParkourDrop();

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool IsParkouring() const { return bIsParkouring; }

	/** Shared by execution and landing previews. Includes overlap and held-marker rearm rules. */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool CanParkourToMarker(ATAParkourMarker* Marker) const;
	bool CanPlayerParkourToMarker(ATAParkourMarker* Marker) const;

	/** 设计开关：默认允许静止时背向起跳并自动转向；不想保留此行为时关闭即可。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Facing")
	bool bAllowStationaryBackFacing = true;

	/** Horizontal speed at/below this value is stationary (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Facing", meta = (ClampMin = "0"))
	float StationarySpeedThreshold = 5.f;

	//用于在动画蓝图中（什么几把中文语法

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool GetParkourFacing() const;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	float GetParkourDuration() const { return ParkourDuration; }

	void RegisterMarker(ATAParkourMarker* Marker);
	void UnregisterMarker(ATAParkourMarker* Marker);
	/** Current held intent; call even while menus block execution so releases can rearm markers. */
	void UpdateHeldRequests(bool bJumpHeld, bool bDropHeld);

protected:
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	ATAParkourMarker* FindCurrentMarkerOfType(ETAParkourMarkerType Type) const;

	bool StartParkour(ATAParkourMarker* Marker);
	void FinishParkour();
	bool HasLanded() const;

	/** 抛物线：t∈[0,1] */
	static FVector EvalParabola(const FVector& Start, const FVector& End, float ArcHeight, float Alpha);

protected:

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
	TSet<TWeakObjectPtr<ATAParkourMarker>> ConsumedHeldMarkers;
};
