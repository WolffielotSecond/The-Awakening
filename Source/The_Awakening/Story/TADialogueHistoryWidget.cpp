// Source/The_Awakening/Story/TADialogueHistoryWidget.cpp
#include "Story/TADialogueHistoryWidget.h"
#include "Core/TALocalizeSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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
