#include "UI/TAPromptWidgetUtils.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

void FTAPromptWidgetUtils::ApplyKeyIcon(
	UImage* Image,
	UTexture2D* KeyIcon,
	float TargetIconHeight,
	USizeBox* IconSizeBox)
{
	if (!Image)
	{
		return;
	}
	if (!KeyIcon)
	{
		Image->SetBrush(FSlateBrush());
		Image->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	const float DrawHeight = FMath::Max(TargetIconHeight, 1.0f);
	const float TextureHeight = static_cast<float>(FMath::Max(KeyIcon->GetSizeY(), 1));
	const float DrawWidth = static_cast<float>(KeyIcon->GetSizeX()) * (DrawHeight / TextureHeight);

	FSlateBrush Brush;
	Brush.SetResourceObject(KeyIcon);
	Brush.ImageSize = FVector2D(DrawWidth, DrawHeight);
	Image->SetBrush(Brush);
	Image->SetDesiredSizeOverride(FVector2D(DrawWidth, DrawHeight));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (!IconSizeBox)
	{
		IconSizeBox = Cast<USizeBox>(Image->GetParent());
	}
	if (IconSizeBox)
	{
		IconSizeBox->SetWidthOverride(DrawWidth);
		IconSizeBox->SetHeightOverride(DrawHeight);
	}
}

void FTAPromptWidgetUtils::ApplyPrompt(
	UUserWidget* Widget,
	UTexture2D* KeyIcon,
	const FText& PromptText,
	float TargetIconHeight)
{
	if (!Widget)
	{
		return;
	}

	if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_Prompt"))))
	{
		TextBlock->SetText(PromptText);
	}

	if (UImage* Image = Cast<UImage>(Widget->GetWidgetFromName(TEXT("Image_Key"))))
	{
		USizeBox* IconSizeBox = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("SizeBox_Key")));
		ApplyKeyIcon(Image, KeyIcon, TargetIconHeight, IconSizeBox);
	}
}
