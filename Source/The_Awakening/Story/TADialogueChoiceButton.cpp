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
		Button_Choice->OnClicked.AddDynamic(this, &UTADialogueChoiceButton::HandleButtonClicked);
	}
	if (Image_Highlight)
	{
		Image_Highlight->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	if (Image_Highlight)
	{
		Image_Highlight->SetVisibility(bHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTADialogueChoiceButton::HandleButtonClicked()
{
	OnClickedIndex.Broadcast(Index);
}
