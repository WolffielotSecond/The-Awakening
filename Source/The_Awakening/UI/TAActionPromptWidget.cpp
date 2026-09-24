#include "UI/TAActionPromptWidget.h"

#include "Core/TAInputIconSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"

void UTAActionPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildFallbackWidget();
	EnsureWidgetBindings();
}

void UTAActionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetBindings();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		InputIconSubsystem = GameInstance->GetSubsystem<UTAInputIconSubsystem>();
		LocalizeSubsystem = GameInstance->GetSubsystem<UTALocalizeSubsystem>();
	}

	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.AddUniqueDynamic(this, &UTAActionPromptWidget::HandleInputDeviceChanged);
	}
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.AddUniqueDynamic(this, &UTAActionPromptWidget::HandleLanguageChanged);
	}
	if (InputIconSubsystem)
	{
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			InputIconSubsystem->RefreshCurrentDeviceForUser(LocalPlayer->GetPlatformUserId());
		}
	}

	RefreshPrompt();
}

void UTAActionPromptWidget::NativeDestruct()
{
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.RemoveDynamic(this, &UTAActionPromptWidget::HandleInputDeviceChanged);
		InputIconSubsystem = nullptr;
	}
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.RemoveDynamic(this, &UTAActionPromptWidget::HandleLanguageChanged);
		LocalizeSubsystem = nullptr;
	}

	Super::NativeDestruct();
}

FReply UTAActionPromptWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnPromptClicked.Broadcast(this);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTAActionPromptWidget::ConfigurePrompt(UInputAction* InAction, FText InActionName)
{
	PromptAction = InAction;
	ActionName = MoveTemp(InActionName);
	ActionNameTextId.Reset();
	bUseLocalizationId = false;
	RefreshPrompt();
}

void UTAActionPromptWidget::ConfigureLocalizedPrompt(UInputAction* InAction, const FString& InActionNameTextId)
{
	PromptAction = InAction;
	ActionNameTextId = InActionNameTextId;
	bUseLocalizationId = !ActionNameTextId.IsEmpty();
	RefreshPrompt();
}

void UTAActionPromptWidget::EnsureWidgetBindings()
{
	if (!Image_Icon)
	{
		Image_Icon = Cast<UImage>(GetWidgetFromName(TEXT("Image_Icon")));
	}
	if (!Text_ActionName)
	{
		Text_ActionName = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_ActionName")));
	}
}

void UTAActionPromptWidget::BuildFallbackWidget()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ActionPromptBackground"));
	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionPromptContent"));
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ActionPromptIconSize"));
	Image_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Icon"));
	Text_ActionName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ActionName"));
	if (!Background || !Content || !IconSize || !Image_Icon || !Text_ActionName)
	{
		return;
	}

	Background->SetBrushColor(FLinearColor(0.025f, 0.03f, 0.04f, 0.82f));
	Background->SetPadding(FMargin(8.0f, 4.0f));
	IconSize->SetWidthOverride(34.0f);
	IconSize->SetHeightOverride(28.0f);
	IconSize->AddChild(Image_Icon);
	Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	Text_ActionName->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = Text_ActionName->GetFont();
	Font.Size = 16;
	Text_ActionName->SetFont(Font);
	Content->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	Content->AddChildToHorizontalBox(Text_ActionName)->SetVerticalAlignment(VAlign_Center);
	Background->SetContent(Content);
	WidgetTree->RootWidget = Background;
}

void UTAActionPromptWidget::RefreshPrompt()
{
	EnsureWidgetBindings();

	if (Text_ActionName)
	{
		if (bUseLocalizationId && LocalizeSubsystem && !ActionNameTextId.IsEmpty())
		{
			Text_ActionName->SetText(LocalizeSubsystem->GetText(ActionNameTextId));
		}
		else
		{
			Text_ActionName->SetText(ActionName);
		}
	}

	if (Image_Icon && InputIconSubsystem)
	{
		Image_Icon->SetBrushFromTexture(PromptAction ? InputIconSubsystem->GetIconForAction(PromptAction) : nullptr);
	}
}

void UTAActionPromptWidget::HandleInputDeviceChanged()
{
	RefreshPrompt();
}

void UTAActionPromptWidget::HandleLanguageChanged()
{
	RefreshPrompt();
}
