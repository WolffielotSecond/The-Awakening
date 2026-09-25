// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Materials/MaterialParameterCollection.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Components/ActorComponent.h"
#include "Scan/TAScanTypes.h"
#include "TAScanningComponent.generated.h"

class ATAScanningActor;
class ATA_HighlightPPActor;
class UTAScannableComponent;
class UTAScanInfoWidget;
class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTAScanStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAScanEnded, ETAScanEndReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTAScanTimeChanged, float, RemainingTime, float, RemainingRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAScanHoverChanged, UTAScannableComponent*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAScanAudioIntensityChanged, float, Intensity);

UENUM(BlueprintType)
enum class ETAScanState : uint8
{
	FadeIn,
	FadedIn,
	FadeOut,
	FadedOut,
	Invalid
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THE_AWAKENING_API UTAScanningComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTAScanningComponent();
	ETAScanState GetScanState() const;
	ATAScanningActor* GetScanPPActor();
	bool StartScan();
	bool EndScan();

	UFUNCTION(BlueprintCallable, Category = "Scan")
	void CancelScan(ETAScanEndReason Reason = ETAScanEndReason::Canceled, bool bRequireInputRelease = true);

	UFUNCTION(BlueprintCallable, Category = "Scan")
	void NotifyScanInputReleased(bool bCanceled = false);

	UFUNCTION(BlueprintPure, Category = "Scan")
	bool IsScanning() const;

	UFUNCTION(BlueprintPure, Category = "Scan")
	float GetScanRemainingTime() const { return ScanRemainingTime; }

	UFUNCTION(BlueprintPure, Category = "Scan")
	float GetScanRemainingRatio() const;

	UFUNCTION(BlueprintPure, Category = "Scan")
	UTAScannableComponent* GetHoveredTarget() const { return HoveredTarget.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Scan|Events")
	FOnTAScanStarted OnScanStarted;

	UPROPERTY(BlueprintAssignable, Category = "Scan|Events")
	FOnTAScanEnded OnScanEnded;

	UPROPERTY(BlueprintAssignable, Category = "Scan|Events")
	FOnTAScanTimeChanged OnScanTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Scan|Events")
	FOnTAScanHoverChanged OnHoveredTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Scan|Events")
	FOnTAScanAudioIntensityChanged OnScanAudioIntensityChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Duration", meta=(ClampMin="0.1"))
	float MaxScanDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Targets", meta=(ClampMin="0.02"))
	float TargetRefreshInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Targets", meta=(ClampMin="1.0"))
	float HoverPixelRadius = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Targets")
	TEnumAsByte<ECollisionChannel> VisibilityTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|UI")
	TSubclassOf<UTAScanInfoWidget> ScanInfoWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|UI")
	FVector2D ScanInfoCursorOffset = FVector2D(20.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Audio")
	TObjectPtr<UCurveFloat> AudioIntensityCurve;

	UFUNCTION(BlueprintPure, Category = "Scan|Audio")
	float GetScanAudioIntensity() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Scan|Audio", meta=(DisplayName="Scan Audio Started"))
	void BP_OnScanAudioStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Scan|Audio", meta=(DisplayName="Scan Audio Updated"))
	void BP_OnScanAudioUpdated(float Intensity);

	UFUNCTION(BlueprintImplementableEvent, Category = "Scan|Audio", meta=(DisplayName="Scan Audio Ended"))
	void BP_OnScanAudioEnded(ETAScanEndReason Reason);

	// Settings | Speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Speed")
	float FadeInSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Speed")
	float FadeOutSpeed = 2.0f;

	// Settings | Actor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Actor")
	bool bDestroyActor = true;

	// Settings | Material
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Material")
	TObjectPtr<UCurveFloat> BlendCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Material")
	TObjectPtr<UMaterialInterface> PostProcessMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Material")
	float Range = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Material")
	float GridCellSize = 500.0f;

	// Settings | Highlight
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Highlight")
	float ShowDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Highlight")
	float HideDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan|Settings|Highlight")
	float HighlightFadeTime = 0.3f;

	//之后技能树要修改这两个变量直接调用这个function即可，ShowDelay减小（按下扫描后物品reveal时间缩短），HideDelay增加（松开扫描后物品会花更长时间才会隐藏）
	UFUNCTION(BlueprintCallable, Category = "Scan|Highlight")
	void SetShowDelay(float NewShowDelay);

	UFUNCTION(BlueprintCallable, Category = "Scan|Highlight")
	void SetHideDelay(float NewHideDelay);

	//升级扫描特效专用，到时候直接call这个就行
	UFUNCTION(BlueprintCallable, Category = "Scan")
	void UpgradeHighlight();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	bool UpdateScanState(ETAScanState NewState, bool bForce);

	bool UpdateScanTime(float DeltaTime);

	void DestroyScanPPActor();

	void UpdateScanPPBlendWeight();

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> ScanParameterCollection;
		

	ETAScanState ScanState = ETAScanState::FadedOut;

	UPROPERTY()
	TObjectPtr<ATAScanningActor> ScanActor;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PostProcessMID;

	UPROPERTY()
	TObjectPtr<APlayerController> PlayerController;

	float ScanNormalizedTime = 0.0f;

	//Highlight

	void UpdateHighlight(float DeltaTime);

	ATA_HighlightPPActor* GetHighlightPPActor();

	void DestroyHighlightPPActor();

	void StartHighlightHideTimer();

	void CancelHighlightHideTimer();

	UPROPERTY()
	TObjectPtr<ATA_HighlightPPActor> HighlightPPActor;

	float HighlightShowProgress = 0.0f;

	FTimerHandle HighlightHideTimerHandle;

	float HighlightValue = 0.0f;

	bool bHighlightFadingIn = false;

	bool bHighlightFadingOut = false;

	void BeginHighlightFadeIn();

	void BeginHighlightFadeOut();

	void StopScan(ETAScanEndReason Reason, bool bRequireInputRelease);
	void UpdateScanDuration(float DeltaTime);
	void RefreshScanTargets();
	void UpdateHoveredTarget();
	void SetHoveredTarget(UTAScannableComponent* NewTarget);
	void UpdateScanInfoWidgetPosition();
	void ClearHighlightedTargets();
	bool HasLineOfSightToTarget(const UTAScannableComponent* Target) const;

	float ScanRemainingTime = 0.0f;
	float TargetRefreshAccumulator = 0.0f;
	bool bScanInputHeld = false;
	bool bWaitingForInputRelease = false;

	TSet<TWeakObjectPtr<UTAScannableComponent>> HighlightedTargets;
	TWeakObjectPtr<UTAScannableComponent> HoveredTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTAScanInfoWidget> ScanInfoWidget;
};
