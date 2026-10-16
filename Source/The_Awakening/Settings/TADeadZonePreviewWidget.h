#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAGamepadDeadZone.h"
#include "TADeadZonePreviewWidget.generated.h"

class UImage;

/** How a dead-zone image follows its value. */
UENUM(BlueprintType)
enum class ETADeadZonePreviewDrive : uint8
{
	/** Scalar parameter on the image's dynamic material (FillParameterName). */
	MaterialParameter,
	/** Render scale (stick: uniform; trigger: Y). The pivot is set in the WBP. */
	RenderScale
};

/** Values one input's images display. */
struct FTADeadZonePreviewState
{
	float MinZone = 0.f;
	float MaxZone = 0.f;
	/** Trigger travel 0-1. */
	float Fill = 0.f;
	/** Stick dot offset from the circle center; screen Y points down. */
	FVector2f DotOffset = FVector2f::ZeroVector;
};

/**
 * Live dead-zone preview in the settings description area. Layout and look live in
 * WBP_DeadZonePreview; this class only drives the bound images every frame:
 * Image_<Input>_MinZone / _MaxZone (dead zones), Image_<Trigger>_Fill (FillAmount = raw
 * travel), Image_<Stick>_Dot (translated by raw input x half the background width).
 * Uses the menu's draft values, so the red areas follow the sliders before Apply.
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADeadZonePreviewWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static FTADeadZonePreviewState ComputeState(ETADeadZoneInput Input, const FTADeadZoneRange& Range, const FVector2D& Raw, float StickRadius);

	/** Dead zones shown: draft values while the settings menu edits, otherwise applied values. */
	FTADeadZoneRange GetDisplayedRange(ETADeadZoneInput Input) const;
	FVector2D GetDisplayedRaw(ETADeadZoneInput Input) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeadZone Preview")
	FName FillParameterName = TEXT("FillAmount");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeadZone Preview")
	ETADeadZonePreviewDrive StickMinZoneDrive = ETADeadZonePreviewDrive::RenderScale;
	/** Stick outer ring always uses the material parameter (a ring cannot be scaled). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeadZone Preview")
	ETADeadZonePreviewDrive TriggerZoneDrive = ETADeadZonePreviewDrive::MaterialParameter;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftTrigger_Background;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftTrigger_MinZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftTrigger_MaxZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftTrigger_Fill;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftStick_Background;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftStick_MinZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftStick_MaxZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_LeftStick_Dot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightStick_Background;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightStick_MinZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightStick_MaxZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightStick_Dot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightTrigger_Background;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightTrigger_MinZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightTrigger_MaxZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RightTrigger_Fill;

private:
	struct FInputImages
	{
		UImage* Background = nullptr;
		UImage* MinZone = nullptr;
		UImage* MaxZone = nullptr;
		/** Trigger fill or stick dot. */
		UImage* Value = nullptr;
	};
	FInputImages GetImages(ETADeadZoneInput Input) const;
	void ApplyMaterialValue(UImage* Image, float Value) const;
	void ApplyScale(UImage* Image, float Value, bool bUniform) const;
	mutable TSet<FName> ReportedProblems;
	void ReportOnce(const FName& Problem, const FString& Message) const;
};
