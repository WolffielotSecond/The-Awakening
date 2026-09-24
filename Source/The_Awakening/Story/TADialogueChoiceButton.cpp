// Source/The_Awakening/Story/TADialogueChoiceButton.cpp
#include "Story/TADialogueChoiceButton.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UTADialogueChoiceButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Choice)
	{
		if (!bHasCachedButtonBackground)
		{
			UnselectedBackgroundColor = Button_Choice->GetBackgroundColor();
			bHasCachedButtonBackground = true;
		}

		// Choice movement and confirmation are routed through Enhanced Input. The
		// dialogue widget leaves choice buttons unfocused to prevent duplicate activation.
		Button_Choice->OnClicked.AddDynamic(this, &UTADialogueChoiceButton::HandleButtonClicked);
	}
	if (Image_Highlight)
	{
		Image_Highlight->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTADialogueChoiceButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnFocusedIndex.Broadcast(Index);
}

void UTADialogueChoiceButton::Setup(int32 InIndex, const FText& Text)
{
	Index = InIndex;
	if (Text_Choice)
	{
		Text_Choice->SetText(Text);
	}
}

void UTADialogueChoiceButton::SetHighlighted(bool bHighlighted)
{
	if (Button_Choice)
	{
		// Keep the selected choice visibly framed even when the optional
		// Image_Highlight widget has not been added to the Blueprint.
		if (!bHasCachedButtonBackground)
		{
			UnselectedBackgroundColor = Button_Choice->GetBackgroundColor();
			bHasCachedButtonBackground = true;
		}

		const FLinearColor SelectedColor(0.12f, 0.42f, 0.82f, 1.0f);
		Button_Choice->SetBackgroundColor(bHighlighted ? SelectedColor : UnselectedBackgroundColor);
	}

	if (Image_Highlight)
	{
		Image_Highlight->SetVisibility(bHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTADialogueChoiceButton::FocusChoice()
{
	if (Button_Choice)
	{
		Button_Choice->SetUserFocus(GetOwningPlayer());
	}
}

void UTADialogueChoiceButton::HandleButtonClicked()
{
	OnClickedIndex.Broadcast(Index);
}
