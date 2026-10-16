#include "UI/TAWorldPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UTAWorldPromptWidget::SetPrompt(UTexture2D* KeyIcon, const FText& PromptText, float IconHeight)
{
	if (Image_Key)
	{
		FTAPromptWidgetUtils::ApplyKeyIcon(Image_Key, KeyIcon, IconHeight, SizeBox_Key);
	}
	if (Text_Prompt)
	{
		Text_Prompt->SetText(PromptText);
	}
	OnPromptChanged(KeyIcon, PromptText);
}
