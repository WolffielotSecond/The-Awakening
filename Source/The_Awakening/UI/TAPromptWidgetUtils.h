#pragma once

#include "CoreMinimal.h"

class UUserWidget;
class UTexture2D;
class UImage;
class USizeBox;

struct THE_AWAKENING_API FTAPromptWidgetUtils
{
	/** Applies a key icon at the requested height while preserving its texture aspect ratio. */
	static void ApplyKeyIcon(UImage* Image, UTexture2D* KeyIcon, float TargetIconHeight, USizeBox* IconSizeBox = nullptr);

	/** 控件名为 Image_Key 和 Text_Prompt */
	static void ApplyPrompt(
		UUserWidget* Widget,
		UTexture2D* KeyIcon,
		const FText& PromptText,
		float TargetIconHeight = 56.f);
};
