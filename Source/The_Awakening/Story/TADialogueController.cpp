// Source/The_Awakening/Story/TADialogueController.cpp
#include "Story/TADialogueController.h"
#include "Story/TADialogueSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAPlayerState.h"
#include "Inventory/TAInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"

UTADialogueController::UTADialogueController()
{
}

void UTADialogueController::Initialize(UTADialogueSubsystem* InSubsystem, const FTAStoryData& InStory, UObject* InInitiator)
{
	Subsystem = InSubsystem;
	Story = &InStory;
	Initiator = InInitiator;
	World = InInitiator ? InInitiator->GetWorld() : (InSubsystem ? InSubsystem->GetWorld() : nullptr);
	CharsPerSecond = InSubsystem ? InSubsystem->CharsPerSecond : 40.f;

	// 语言切换时刷新当前展示内容（历史/选项存 Key，UI 重新解析）
	if (UWorld* W = World.Get())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				Loc->OnLanguageChanged.AddDynamic(this, &UTADialogueController::HandleLanguageChanged);
			}
		}
	}
}

void UTADialogueController::Shutdown()
{
	bActive = false;
	bTyping = false;

	if (UWorld* W = World.Get())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				Loc->OnLanguageChanged.RemoveDynamic(this, &UTADialogueController::HandleLanguageChanged);
			}
		}
	}

	OnLineShown.Clear();
	OnChoicesChanged.Clear();
	OnPortraitsChanged.Clear();
	OnSpeakerChanged.Clear();
	OnHistoryChanged.Clear();
	OnStoryFinished.Clear();

	World.Reset();
	Initiator.Reset();
	Subsystem = nullptr;
}

void UTADialogueController::Start()
{
	if (!Story || bActive)
	{
		return;
	}

	bActive = true;

	const int32 EntryIdx = Story->FindNodeIndex(Story->Entry);
	if (EntryIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 入口节点不存在: %s"), *Story->Entry);
		Finish();
		return;
	}

	EnterNode(EntryIdx);
}

// ------------------------------------------------------------
// 玩家操作
// ------------------------------------------------------------

void UTADialogueController::Advance()
{
	if (!bActive)
	{
		return;
	}

	// 打字中：立即显示全文
	if (bTyping)
	{
		CompleteLine();
		return;
	}

	// 分支界面：继续键无效（由选项驱动）
	if (IsChoiceNode())
	{
		return;
	}

	GoToNext();
}

void UTADialogueController::SelectChoice(int32 VisibleIndex)
{
	if (!bActive || !IsChoiceNode())
	{
		return;
	}

	if (!VisibleChoiceIndices.IsValidIndex(VisibleIndex))
	{
		return;
	}

	const FTAStoryNode& Node = Story->Nodes[CurrentNodeIndex];
	const FTAStoryChoice& Choice = Node.Choices[VisibleChoiceIndices[VisibleIndex]];

	// 历史：记录玩家选择
	FTAStoryHistoryEntry Entry;
	Entry.TextId = Choice.TextId;
	Entry.bIsChoice = true;
	HistoryEntries.Add(Entry);
	OnHistoryChanged.Broadcast();

	RunEvents(Choice.Events);
	RunEvents(Node.EventsOnExit);

	if (Choice.Target.IsEmpty())
	{
		Finish();
		return;
	}

	const int32 TargetIdx = Story->FindNodeIndex(Choice.Target);
	if (TargetIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 选项目标节点不存在: %s"), *Choice.Target);
		Finish();
		return;
	}

	EnterNode(TargetIdx);
}

void UTADialogueController::MoveSelection(int32 Delta)
{
	if (VisibleChoiceIndices.Num() == 0)
	{
		return;
	}

	SelectedChoiceIndex = (SelectedChoiceIndex + Delta) % VisibleChoiceIndices.Num();
	if (SelectedChoiceIndex < 0)
	{
		SelectedChoiceIndex += VisibleChoiceIndices.Num();
	}

	OnChoicesChanged.Broadcast();
}

void UTADialogueController::SetSelectedChoiceIndex(int32 VisibleIndex)
{
	if (!bActive || !IsChoiceNode() || !VisibleChoiceIndices.IsValidIndex(VisibleIndex))
	{
		return;
	}
	SelectedChoiceIndex = VisibleIndex;
}

// ------------------------------------------------------------
// Tick（打字机）
// ------------------------------------------------------------

void UTADialogueController::Tick(float DeltaTime)
{
	if (!bActive || !bTyping)
	{
		return;
	}

	VisibleCharCount += CharsPerSecond * DeltaTime;

	const int32 TotalLen = CurrentFullText.ToString().Len();
	if ((int32)VisibleCharCount >= TotalLen)
	{
		CompleteLine();
	}
}

// ------------------------------------------------------------
// 状态查询
// ------------------------------------------------------------

bool UTADialogueController::IsChoiceNode() const
{
	if (!bActive || !Story || !Story->Nodes.IsValidIndex(CurrentNodeIndex))
	{
		return false;
	}
	return Story->Nodes[CurrentNodeIndex].Type == TEXT("choice");
}

FText UTADialogueController::GetVisibleText() const
{
	const FString Full = CurrentFullText.ToString();
	const int32 Chars = FMath::Clamp((int32)VisibleCharCount, 0, Full.Len());
	return FText::FromString(Full.Left(Chars));
}

FString UTADialogueController::GetStoryId() const
{
	return Story ? Story->StoryId : FString();
}

TArray<FText> UTADialogueController::GetVisibleChoiceTexts() const
{
	TArray<FText> Texts;
	if (!Story || !Story->Nodes.IsValidIndex(CurrentNodeIndex))
	{
		return Texts;
	}

	const FTAStoryNode& Node = Story->Nodes[CurrentNodeIndex];
	for (const int32 ChoiceIdx : VisibleChoiceIndices)
	{
		if (Node.Choices.IsValidIndex(ChoiceIdx))
		{
			Texts.Add(ResolveText(Node.Choices[ChoiceIdx].TextId));
		}
	}
	return Texts;
}

// ------------------------------------------------------------
// 编辑器预览
// ------------------------------------------------------------

void UTADialogueController::SetConditionOverrides(const FTADialogueConditionContext& InOverrides)
{
	OverrideContext = InOverrides;
	bOverrideContext = true;

	// 重算当前分支可见性（若正在分支界面）
	if (bActive && IsChoiceNode())
	{
		PresentChoices(Story->Nodes[CurrentNodeIndex]);
	}
}

// ------------------------------------------------------------
// 流程
// ------------------------------------------------------------

void UTADialogueController::EnterNode(int32 NodeIndex)
{
	CurrentNodeIndex = NodeIndex;
	const FTAStoryNode& Node = Story->Nodes[NodeIndex];

	// Leave no stale choice buttons visible after selecting an option and entering a non-choice node.
	if (Node.Type != TEXT("choice"))
	{
		OnChoicesChanged.Broadcast();
	}

	// 进入事件
	RunEvents(Node.EventsOnEnter);

	// 立绘更新
	ApplyPortraitEntries(Node.Portraits);

	// 说话者
	if (SpeakerId != Node.SpeakerId)
	{
		SpeakerId = Node.SpeakerId;
		OnSpeakerChanged.Broadcast();
	}

	NotifyProgress();

	if (Node.Type == TEXT("dialogue"))
	{
		BeginLine(Node);
	}
	else if (Node.Type == TEXT("choice"))
	{
		PresentChoices(Node);
	}
	else if (Node.Type == TEXT("end"))
	{
		Finish();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 未知节点类型 %s（节点 %s），结束对话"), *Node.Type, *Node.Id);
		Finish();
	}
}

void UTADialogueController::BeginLine(const FTAStoryNode& Node)
{
	CurrentFullText = ResolveText(Node.TextId);
	CurrentSpeakerName = ResolveText(Node.SpeakerNameId);

	VisibleCharCount = 0.f;
	bTyping = true;
	if (CharsPerSecond <= 0.f)
	{
		VisibleCharCount = (float)CurrentFullText.ToString().Len();
		bTyping = false;
	}

	OnLineShown.Broadcast();
}

void UTADialogueController::CompleteLine()
{
	if (!bActive || !Story || !Story->Nodes.IsValidIndex(CurrentNodeIndex))
	{
		return;
	}

	bTyping = false;
	VisibleCharCount = (float)CurrentFullText.ToString().Len();

	// 历史：只在完整显示后记录
	const FTAStoryNode& Node = Story->Nodes[CurrentNodeIndex];
	FTAStoryHistoryEntry Entry;
	Entry.SpeakerNameId = Node.SpeakerNameId;
	Entry.TextId = Node.TextId;
	Entry.bIsChoice = false;
	HistoryEntries.Add(Entry);
	OnHistoryChanged.Broadcast();
}

void UTADialogueController::GoToNext()
{
	if (!bActive || !Story || !Story->Nodes.IsValidIndex(CurrentNodeIndex))
	{
		return;
	}

	const FTAStoryNode& Node = Story->Nodes[CurrentNodeIndex];
	RunEvents(Node.EventsOnExit);

	if (Node.Next.IsEmpty())
	{
		Finish();
		return;
	}

	const int32 NextIdx = Story->FindNodeIndex(Node.Next);
	if (NextIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 下一节点不存在: %s"), *Node.Next);
		Finish();
		return;
	}

	EnterNode(NextIdx);
}

void UTADialogueController::PresentChoices(const FTAStoryNode& Node)
{
	VisibleChoiceIndices.Reset();

	// 1) 收集所有条件满足的选项
	for (int32 i = 0; i < Node.Choices.Num(); ++i)
	{
		if (AllConditionsTrue(Node.Choices[i].Conditions))
		{
			VisibleChoiceIndices.Add(i);
		}
	}

	// 2) 一个都不满足：显示无条件的兜底选项
	if (VisibleChoiceIndices.Num() == 0)
	{
		for (int32 i = 0; i < Node.Choices.Num(); ++i)
		{
			if (Node.Choices[i].Conditions.Num() == 0)
			{
				VisibleChoiceIndices.Add(i);
			}
		}
	}

	// 3) 仍然没有：警告并结束
	if (VisibleChoiceIndices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 节点 %s 无可见选项，结束对话"), *Node.Id);
		Finish();
		return;
	}

	SelectedChoiceIndex = 0;
	OnChoicesChanged.Broadcast();
}

void UTADialogueController::Finish()
{
	if (!bActive)
	{
		return;
	}

	bActive = false;
	bTyping = false;

	if (Story && Subsystem)
	{
		Subsystem->MarkStoryCompleted(Story->StoryId);
	}

	OnStoryFinished.Broadcast();
}

// ------------------------------------------------------------
// 立绘
// ------------------------------------------------------------

void UTADialogueController::ApplyPortraitEntries(const TArray<FTAPortraitEntry>& Entries)
{
	bool bChanged = false;

	for (const FTAPortraitEntry& Entry : Entries)
	{
		if (Entry.CharacterId.IsEmpty())
		{
			continue;
		}

		FTAPortraitEntry* Existing = ActivePortraits.FindByPredicate(
			[&](const FTAPortraitEntry& E) { return E.CharacterId == Entry.CharacterId; });

		// 显式移除
		if (Entry.bVisibleSpecified && !Entry.bVisible)
		{
			if (Existing)
			{
				ActivePortraits.RemoveAt(Existing - ActivePortraits.GetData());
				bChanged = true;
			}
			continue;
		}

		if (Existing)
		{
			// 只覆盖显式指定的字段（表情差分）
			if (Entry.bImagesSpecified)
			{
				if (!Entry.Base.IsNull())        Existing->Base = Entry.Base;
				if (!Entry.EyesOpen.IsNull())    Existing->EyesOpen = Entry.EyesOpen;
				if (!Entry.EyesClosed.IsNull())  Existing->EyesClosed = Entry.EyesClosed;
				if (!Entry.MouthOpen.IsNull())   Existing->MouthOpen = Entry.MouthOpen;
				if (!Entry.MouthClosed.IsNull()) Existing->MouthClosed = Entry.MouthClosed;
			}
			if (Entry.bPositionSpecified)     Existing->Position = Entry.Position;
			if (Entry.bScaleSpecified)        Existing->Scale = Entry.Scale;
			if (Entry.bVisibleSpecified)      Existing->bVisible = Entry.bVisible;
			bChanged = true;
		}
		else
		{
			ActivePortraits.Add(Entry);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnPortraitsChanged.Broadcast();
	}
}

// ------------------------------------------------------------
// 事件 / 条件 / 进度
// ------------------------------------------------------------

void UTADialogueController::RunEvents(const TArray<FTAStoryEvent>& Events)
{
	for (const FTAStoryEvent& Event : Events)
	{
		if (Event.Name.IsEmpty())
		{
			continue;
		}
		if (Subsystem)
		{
			Subsystem->BroadcastStoryEvent(FName(*Event.Name), Event.Params);
		}
	}
}

FTADialogueConditionContext UTADialogueController::BuildContext() const
{
	FTADialogueConditionContext Ctx;

	if (bOverrideContext)
	{
		Ctx = OverrideContext;
	}
	else
	{
		Ctx.World = World.Get();
		if (UWorld* W = World.Get())
		{
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				Ctx.PlayerState = PC->GetPlayerState<ATAPlayerState>();
				if (APawn* Pawn = PC->GetPawn())
				{
					Ctx.Inventory = Pawn->FindComponentByClass<UTAInventoryComponent>();
				}
			}
		}
	}

	Ctx.Dialogue = Subsystem;
	return Ctx;
}

bool UTADialogueController::AllConditionsTrue(const TArray<FTAStoryCondition>& Conditions) const
{
	if (!Subsystem)
	{
		return false;
	}

	const FTADialogueConditionContext Ctx = BuildContext();
	for (const FTAStoryCondition& Condition : Conditions)
	{
		if (!Subsystem->EvaluateCondition(Condition, Ctx))
		{
			return false;
		}
	}
	return true;
}

void UTADialogueController::NotifyProgress()
{
	if (Story && Subsystem && Story->Nodes.IsValidIndex(CurrentNodeIndex))
	{
		Subsystem->UpdateStoryProgress(Story->StoryId, Story->Nodes[CurrentNodeIndex].Id);
	}
}

FText UTADialogueController::ResolveText(const FString& TextId) const
{
	if (TextId.IsEmpty())
	{
		return FText::GetEmpty();
	}

	if (UWorld* W = World.Get())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				return Loc->GetText(TextId);
			}
		}
	}

	return FText::FromString(TextId);
}

void UTADialogueController::HandleLanguageChanged()
{
	if (!bActive || !Story || !Story->Nodes.IsValidIndex(CurrentNodeIndex))
	{
		return;
	}

	const FTAStoryNode& Node = Story->Nodes[CurrentNodeIndex];

	if (Node.Type == TEXT("dialogue"))
	{
		CurrentFullText = ResolveText(Node.TextId);
		CurrentSpeakerName = ResolveText(Node.SpeakerNameId);
		OnLineShown.Broadcast();
	}
	else if (Node.Type == TEXT("choice"))
	{
		OnChoicesChanged.Broadcast();
	}

	OnHistoryChanged.Broadcast();
}
