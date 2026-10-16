#include "Settings/TADeadZonePreviewWidget.h"
#include "Settings/TASettingsSubsystem.h"
#include "The_AwakeningPlayerController.h"
#include "Components/Image.h"
#include "Engine/LocalPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogTADeadZonePreview, Log, All);

namespace
{
	bool IsStick(ETADeadZoneInput Input)
	{
		return Input == ETADeadZoneInput::LeftStick || Input == ETADeadZoneInput::RightStick;
	}
	const TCHAR* InputName(ETADeadZoneInput Input)
	{
		switch (Input)
		{
		case ETADeadZoneInput::LeftTrigger: return TEXT("LeftTrigger");
		case ETADeadZoneInput::LeftStick: return TEXT("LeftStick");
		case ETADeadZoneInput::RightStick: return TEXT("RightStick");
		default: return TEXT("RightTrigger");
		}
	}
}

FTADeadZonePreviewState UTADeadZonePreviewWidget::ComputeState(ETADeadZoneInput Input, const FTADeadZoneRange& Range, const FVector2D& Raw, float StickRadius)
{
	FTADeadZonePreviewState State;
	State.MinZone = FMath::Clamp(Range.Min, 0.f, 1.f);
	State.MaxZone = FMath::Clamp(Range.Max, 0.f, 1.f - State.MinZone);
	if (IsStick(Input))
	{
		const FVector2D Clamped = Raw.GetClampedToMaxSize(1.0);
		State.DotOffset = FVector2f(static_cast<float>(Clamped.X), static_cast<float>(-Clamped.Y)) * StickRadius;
	}
	else
	{
		State.Fill = FMath::Clamp(static_cast<float>(FMath::Abs(Raw.X)), 0.f, 1.f);
	}
	return State;
}

FTADeadZoneRange UTADeadZonePreviewWidget::GetDisplayedRange(ETADeadZoneInput Input) const
{
	FTADeadZoneRange Range;
	if (const auto* Controller = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		Range = Controller->GetGamepadDeadZone().GetRange(Input);
	}
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	if (const auto* Settings = Player ? Player->GetSubsystem<UTASettingsSubsystem>() : nullptr)
	{
		auto Read = [Settings](FName Id, float Fallback)
		{
			const FTASettingDefinition* Definition = Settings->FindEditableDefinition(Id);
			return Definition ? Definition->ToSliderNumber(Settings->GetEditableInteger(Id, Definition->DefaultValue)) : Fallback;
		};
		Range.Min = Read(FTAGamepadDeadZone::GetSettingId(Input, false), Range.Min);
		Range.Max = Read(FTAGamepadDeadZone::GetSettingId(Input, true), Range.Max);
	}
	return Range;
}

FVector2D UTADeadZonePreviewWidget::GetDisplayedRaw(ETADeadZoneInput Input) const
{
	const auto* Controller = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
	return Controller ? Controller->GetGamepadDeadZone().GetRaw(Input) : FVector2D::ZeroVector;
}

UTADeadZonePreviewWidget::FInputImages UTADeadZonePreviewWidget::GetImages(ETADeadZoneInput Input) const
{
	switch (Input)
	{
	case ETADeadZoneInput::LeftTrigger: return {Image_LeftTrigger_Background, Image_LeftTrigger_MinZone, Image_LeftTrigger_MaxZone, Image_LeftTrigger_Fill};
	case ETADeadZoneInput::LeftStick: return {Image_LeftStick_Background, Image_LeftStick_MinZone, Image_LeftStick_MaxZone, Image_LeftStick_Dot};
	case ETADeadZoneInput::RightStick: return {Image_RightStick_Background, Image_RightStick_MinZone, Image_RightStick_MaxZone, Image_RightStick_Dot};
	default: return {Image_RightTrigger_Background, Image_RightTrigger_MinZone, Image_RightTrigger_MaxZone, Image_RightTrigger_Fill};
	}
}

void UTADeadZonePreviewWidget::ReportOnce(const FName& Problem, const FString& Message) const
{
	if (ReportedProblems.Contains(Problem)) return;
	ReportedProblems.Add(Problem);
	UE_LOG(LogTADeadZonePreview, Warning, TEXT("%s: %s"), *GetClass()->GetName(), *Message);
}

void UTADeadZonePreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	for (int32 Index = 0; Index < FTAGamepadDeadZone::InputCount; ++Index)
	{
		const auto Input = static_cast<ETADeadZoneInput>(Index);
		const FInputImages Images = GetImages(Input);
		const FString Prefix = FString::Printf(TEXT("Image_%s_"), InputName(Input));
		const TPair<UImage*, const TCHAR*> Required[] = {
			{Images.Background, TEXT("Background")}, {Images.MinZone, TEXT("MinZone")}, {Images.MaxZone, TEXT("MaxZone")},
			{Images.Value, IsStick(Input) ? TEXT("Dot") : TEXT("Fill")}};
		for (const auto& Pair : Required)
		{
			if (!Pair.Key) ReportOnce(FName(Prefix + Pair.Value), FString::Printf(TEXT("missing image %s%s"), *Prefix, Pair.Value));
		}
	}
}

void UTADeadZonePreviewWidget::ApplyMaterialValue(UImage* Image, float Value) const
{
	if (!Image) return;
	if (UMaterialInstanceDynamic* Material = Image->GetDynamicMaterial())
	{
		Material->SetScalarParameterValue(FillParameterName, Value);
	}
	else
	{
		ReportOnce(Image->GetFName(), FString::Printf(TEXT("%s needs a material brush with scalar parameter %s"), *Image->GetName(), *FillParameterName.ToString()));
	}
}

void UTADeadZonePreviewWidget::ApplyScale(UImage* Image, float Value, bool bUniform) const
{
	if (Image) Image->SetRenderScale(bUniform ? FVector2D(Value, Value) : FVector2D(1.0, Value));
}

void UTADeadZonePreviewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	for (int32 Index = 0; Index < FTAGamepadDeadZone::InputCount; ++Index)
	{
		const auto Input = static_cast<ETADeadZoneInput>(Index);
		const FInputImages Images = GetImages(Input);
		const float Radius = Images.Background ? Images.Background->GetCachedGeometry().GetLocalSize().X * 0.5f : 0.f;
		const FTADeadZonePreviewState State = ComputeState(Input, GetDisplayedRange(Input), GetDisplayedRaw(Input), Radius);
		if (IsStick(Input))
		{
			if (StickMinZoneDrive == ETADeadZonePreviewDrive::RenderScale) ApplyScale(Images.MinZone, State.MinZone, true);
			else ApplyMaterialValue(Images.MinZone, State.MinZone);
			ApplyMaterialValue(Images.MaxZone, State.MaxZone);
			if (Images.Value) Images.Value->SetRenderTranslation(FVector2D(State.DotOffset));
		}
		else
		{
			if (TriggerZoneDrive == ETADeadZonePreviewDrive::RenderScale)
			{
				ApplyScale(Images.MinZone, State.MinZone, false);
				ApplyScale(Images.MaxZone, State.MaxZone, false);
			}
			else
			{
				ApplyMaterialValue(Images.MinZone, State.MinZone);
				ApplyMaterialValue(Images.MaxZone, State.MaxZone);
			}
			ApplyMaterialValue(Images.Value, State.Fill);
		}
	}
}
