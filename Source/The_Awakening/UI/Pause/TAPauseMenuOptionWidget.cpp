#include "UI/Pause/TAPauseMenuOptionWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UTAPauseMenuOptionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Option)
	{
		if (!bHasCachedButtonBackground)
		{
			UnselectedBackgroundColor = Button_Option->GetBackgroundColor();
			bHasCachedButtonBackground = true;
		}
		Button_Option->OnClicked.AddUniqueDynamic(this, &UTAPauseMenuOptionWidget::HandleButtonClicked);
	}
	if (Image_Highlight)
	{
		Image_Highlight->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTAPauseMenuOptionWidget::ConfigureOption(FName InOptionId, const FText& InLabel)
{
	CurrentOptionId = InOptionId;
	if (Text_Option)
	{
		Text_Option->SetText(InLabel);
	}
}

void UTAPauseMenuOptionWidget::SetHighlighted(bool bHighlighted)
{
	if (Button_Option)
	{
		if (!bHasCachedButtonBackground)
		{
			UnselectedBackgroundColor = Button_Option->GetBackgroundColor();
			bHasCachedButtonBackground = true;
		}
		const FLinearColor SelectedColor(0.12f, 0.42f, 0.82f, 1.0f);
		Button_Option->SetBackgroundColor(bHighlighted ? SelectedColor : UnselectedBackgroundColor);
	}
	if (Image_Highlight)
	{
		Image_Highlight->SetVisibility(bHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UWidget* UTAPauseMenuOptionWidget::GetFocusTarget() const
{
	return Button_Option;
}

void UTAPauseMenuOptionWidget::HandleButtonClicked()
{
	if (!CurrentOptionId.IsNone())
	{
		OnOptionSelected.Broadcast(CurrentOptionId);
	}
}
