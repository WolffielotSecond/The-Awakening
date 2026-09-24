// Source/The_Awakening/Story/TADialogueHistoryWidget.cpp
#include "Story/TADialogueHistoryWidget.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "InputAction.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	template <typename WidgetType>
	WidgetType* FindHistoryWidget(UUserWidget* Owner, const TCHAR* WidgetName)
	{
		if (!Owner)
		{
			return nullptr;
		}

		if (WidgetType* Found = Cast<WidgetType>(Owner->GetWidgetFromName(WidgetName)))
		{
			return Found;
		}

		WidgetType* RecursiveResult = nullptr;
		if (Owner->WidgetTree)
		{
			const FName TargetName(WidgetName);
			Owner->WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Widget)
			{
				if (!RecursiveResult && Widget && Widget->GetFName() == TargetName)
				{
					RecursiveResult = Cast<WidgetType>(Widget);
				}
			});
		}
		return RecursiveResult;
	}
}

void UTADialogueHistoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!Button_Close) Button_Close = FindHistoryWidget<UButton>(this, TEXT("Button_Close"));
	if (!Text_CloseText) Text_CloseText = FindHistoryWidget<UTextBlock>(this, TEXT("Text_CloseText"));
	if (!Image_CloseIcon) Image_CloseIcon = FindHistoryWidget<UImage>(this, TEXT("Image_CloseIcon"));
	if (Button_Close)
	{
		Button_Close->OnClicked.AddDynamic(this, &UTADialogueHistoryWidget::HandleCloseClicked);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		LocalizeSubsystem = GI->GetSubsystem<UTALocalizeSubsystem>();
		InputIconSubsystem = GI->GetSubsystem<UTAInputIconSubsystem>();
	}
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.AddDynamic(this, &UTADialogueHistoryWidget::HandleLanguageChanged);
	}
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.AddDynamic(this, &UTADialogueHistoryWidget::HandleInputDeviceChanged);
	}
	RefreshClosePrompt();
}

void UTADialogueHistoryWidget::NativeDestruct()
{
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveDynamic(this, &UTADialogueHistoryWidget::HandleCloseClicked);
	}
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.RemoveDynamic(this, &UTADialogueHistoryWidget::HandleLanguageChanged);
		LocalizeSubsystem = nullptr;
	}
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.RemoveDynamic(this, &UTADialogueHistoryWidget::HandleInputDeviceChanged);
		InputIconSubsystem = nullptr;
	}
	Super::NativeDestruct();
}

void UTADialogueHistoryWidget::SetupClosePrompt(UInputAction* InCloseAction)
{
	CloseHistoryAction = InCloseAction;
	RefreshClosePrompt();
}

void UTADialogueHistoryWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UTADialogueHistoryWidget::RefreshClosePrompt()
{
	if (Text_CloseText && LocalizeSubsystem)
	{
		Text_CloseText->SetText(LocalizeSubsystem->GetText(TEXT("UI_Dialogue_CloseHistory")));
	}
	if (Image_CloseIcon && InputIconSubsystem)
	{
		Image_CloseIcon->SetBrushFromTexture(CloseHistoryAction ? InputIconSubsystem->GetIconForAction(CloseHistoryAction) : nullptr);
	}
}

void UTADialogueHistoryWidget::HandleLanguageChanged()
{
	RefreshClosePrompt();
}

void UTADialogueHistoryWidget::HandleInputDeviceChanged()
{
	RefreshClosePrompt();
}

void UTADialogueHistoryWidget::SetHistory(const TArray<FTAStoryHistoryEntry>& InHistory)
{
	History = InHistory;
	Rebuild();
}

void UTADialogueHistoryWidget::Rebuild()
{
	if (!Box_Entries)
	{
		return;
	}

	Box_Entries->ClearChildren();

	for (const FTAStoryHistoryEntry& Entry : History)
	{
		UTextBlock* Text = NewObject<UTextBlock>(this);

		FString Line;
		if (Entry.bIsChoice)
		{
			Line = FString::Printf(TEXT("▶ %s"), *ResolveText(Entry.TextId).ToString());
		}
		else if (!Entry.SpeakerNameId.IsEmpty())
		{
			Line = FString::Printf(TEXT("%s：%s"),
				*ResolveText(Entry.SpeakerNameId).ToString(),
				*ResolveText(Entry.TextId).ToString());
		}
		else
		{
			Line = ResolveText(Entry.TextId).ToString();
		}

		Text->SetText(FText::FromString(Line));
		Box_Entries->AddChildToVerticalBox(Text);
	}

	if (ScrollBox_History)
	{
		ScrollBox_History->ScrollToEnd();
	}
}

FText UTADialogueHistoryWidget::ResolveText(const FString& TextId) const
{
	if (TextId.IsEmpty())
	{
		return FText::GetEmpty();
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				return Loc->GetText(TextId);
			}
		}
	}

	return FText::FromString(TextId);
}
