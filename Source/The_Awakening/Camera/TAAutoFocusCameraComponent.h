#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "TAAutoFocusCameraComponent.generated.h"

/** Character focus evaluated when the current camera view is requested, after spring-arm movement. */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAAutoFocusCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Auto Focus")
	bool bAutoFocusEnabled = true;

	/** World-space position of this component is the focus target. Null leaves manual DOF untouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Auto Focus")
	TObjectPtr<USceneComponent> FocusPoint;

	/** FInterpTo speed. Zero snaps immediately to the target (Unreal convention). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Auto Focus", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FocusInterpSpeed = 8.f;

	/** Applied focus distance in cm; only the focal-distance field of the outgoing view is overridden. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Camera|Auto Focus")
	float CurrentFocalDistance = 0.f;

	/** Optical-axis distance in cm before interpolation, clamped to at least 1 cm. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Camera|Auto Focus")
	float TargetFocalDistance = 0.f;

	/** Initialize directly from the next valid camera view, e.g. after an intentional camera cut. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Auto Focus")
	void ResetAutoFocus();

private:
	bool bFocusInitialized = false;
};
