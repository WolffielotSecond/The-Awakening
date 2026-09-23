// Source/The_Awakening/Story/TADialogueSubsystem.cpp
#include "Story/TADialogueSubsystem.h"
#include "Story/TADialogueController.h"
#include "Story/TADialogueWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

// ------------------------------------------------------------
// 生命周期
// ------------------------------------------------------------

void UTADialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 内置条件求值器
	RegisterConditionEvaluator(MakeShared<FTADialogueFlagEvaluator>());
	RegisterConditionEvaluator(MakeShared<FTADialogueHasItemEvaluator>());
	RegisterConditionEvaluator(MakeShared<FTADialogueMoneyEvaluator>());
	RegisterConditionEvaluator(MakeShared<FTADialogueAttributeEvaluator>());

	// 内置保留名处理器：SetFlag（分支条件依赖旗标，必须由剧情系统自己可靠维护）
	RegisterReservedEventHandler(TEXT("SetFlag"),
		[this](const FTAStoryEventPayload& Payload) { HandleReservedSetFlag(Payload); });
}

void UTADialogueSubsystem::Deinitialize()
{
	if (ActiveController)
	{
		StopDialogue();
	}
	Super::Deinitialize();
}

// ------------------------------------------------------------
// 旗标
// ------------------------------------------------------------

int32 UTADialogueSubsystem::GetFlagInt(const FString& Flag) const
{
	const int32* Found = Flags.Find(Flag);
	return Found ? *Found : 0;
}

void UTADialogueSubsystem::SetFlagInt(const FString& Flag, int32 Value)
{
	if (Flag.IsEmpty())
	{
		return;
	}
	Flags.Add(Flag, Value);
}

bool UTADialogueSubsystem::GetFlagBool(const FString& Flag) const
{
	return GetFlagInt(Flag) != 0;
}

void UTADialogueSubsystem::SetFlagBool(const FString& Flag, bool bValue)
{
	SetFlagInt(Flag, bValue ? 1 : 0);
}

// ------------------------------------------------------------
// 条件
// ------------------------------------------------------------

void UTADialogueSubsystem::RegisterConditionEvaluator(TSharedPtr<ITADialogueConditionEvaluator> Evaluator)
{
	if (!Evaluator.IsValid())
	{
		return;
	}

	for (TSharedPtr<ITADialogueConditionEvaluator>& Existing : ConditionEvaluators)
	{
		if (Existing->GetTypeName().Equals(Evaluator->GetTypeName(), ESearchCase::IgnoreCase))
		{
			Existing = Evaluator;
			return;
		}
	}

	ConditionEvaluators.Add(Evaluator);
}

bool UTADialogueSubsystem::EvaluateCondition(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const
{
	for (const TSharedPtr<ITADialogueConditionEvaluator>& Evaluator : ConditionEvaluators)
	{
		if (Evaluator->GetTypeName().Equals(Condition.Type, ESearchCase::IgnoreCase))
		{
			return Evaluator->Evaluate(Condition, Context);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 未知条件类型: %s"), *Condition.Type);
	return false;
}

// ------------------------------------------------------------
// 事件广播
// ------------------------------------------------------------

void UTADialogueSubsystem::RegisterReservedEventHandler(FName EventName, TFunction<void(const FTAStoryEventPayload&)> Handler)
{
	if (!EventName.IsNone())
	{
		ReservedHandlers.Add(EventName, Handler);
	}
}

void UTADialogueSubsystem::HandleReservedSetFlag(const FTAStoryEventPayload& Payload)
{
	const FString* FlagPtr = Payload.Params.Find(TEXT("flag"));
	if (!FlagPtr || FlagPtr->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] SetFlag 事件缺少 flag 参数"));
		return;
	}

	const FString* ValuePtr = Payload.Params.Find(TEXT("value"));
	if (!ValuePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] SetFlag 事件缺少 value 参数"));
		return;
	}

	// "true"/"false" 按布尔处理，其余按整数
	if (ValuePtr->Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
		ValuePtr->Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		SetFlagBool(*FlagPtr, ValuePtr->Equals(TEXT("true"), ESearchCase::IgnoreCase));
	}
	else
	{
		SetFlagInt(*FlagPtr, FCString::Atoi(**ValuePtr));
	}
}

void UTADialogueSubsystem::BroadcastStoryEvent(FName EventName, const TMap<FString, FString>& Params)
{
	const FTAStoryEventPayload Payload(EventName.ToString(), Params);

	// 1) 保留名内部处理器（如 SetFlag）
	if (const TFunction<void(const FTAStoryEventPayload&)>* Handler = ReservedHandlers.Find(EventName))
	{
		(*Handler)(Payload);
	}

	// 2) 广播给所有外部监听者（蓝图/C++）
	OnStoryEvent.Broadcast(Payload);
}

// ------------------------------------------------------------
// 剧情加载与解析
// ------------------------------------------------------------

FString UTADialogueSubsystem::GetStoryFilePath(const FString& StoryId) const
{
	FString CleanId = StoryId;
	if (CleanId.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		CleanId.LeftChopInline(5);
	}
	return FPaths::ProjectContentDir() / StoriesFolder / (CleanId + TEXT(".json"));
}

const FTAStoryData* UTADialogueSubsystem::LoadStory(const FString& StoryId)
{
	FString CleanId = StoryId;
	if (CleanId.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		CleanId.LeftChopInline(5);
	}

	if (const FTAStoryData* Cached = LoadedStories.Find(CleanId))
	{
		return Cached;
	}

	const FString FilePath = GetStoryFilePath(CleanId);
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 找不到剧情文件: %s"), *FilePath);
		return nullptr;
	}

	FTAStoryData Parsed;
	FString Error;
	if (!ParseStoryJson(JsonString, CleanId, Parsed, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 剧情解析失败 %s: %s"), *CleanId, *Error);
		return nullptr;
	}

	return &LoadedStories.Add(CleanId, MoveTemp(Parsed));
}

FString UTADialogueSubsystem::JsonScalarToString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::String:
	{
		FString S;
		Value->TryGetString(S);
		return S;
	}
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Number:
	{
		const double D = Value->AsNumber();
		if (FMath::Frac(D) == 0.0)
		{
			return FString::Printf(TEXT("%lld"), (int64)D);
		}
		return FString::SanitizeFloat(D);
	}
	default:
		return FString();
	}
}

// ------------------------------------------------------------
// JSON 解析
// ------------------------------------------------------------

namespace
{
	bool ParseEventJson(const TSharedPtr<FJsonObject>& Obj, FTAStoryEvent& Out)
	{
		Out.Name = Obj->GetStringField(TEXT("name"));
		if (Out.Name.IsEmpty())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("params"), ParamsObj))
		{
			for (const auto& Pair : (*ParamsObj)->Values)
			{
				Out.Params.Add(Pair.Key, UTADialogueSubsystem::JsonScalarToString(Pair.Value));
			}
		}
		return true;
	}

	bool ParseConditionJson(const TSharedPtr<FJsonObject>& Obj, FTAStoryCondition& Out)
	{
		Out.Type = Obj->GetStringField(TEXT("type"));
		if (Out.Type.IsEmpty())
		{
			return false;
		}

		Out.Flag = Obj->GetStringField(TEXT("flag"));
		Out.Item = Obj->GetStringField(TEXT("item"));
		Out.Attribute = Obj->GetStringField(TEXT("attribute"));

		FString Op;
		if (Obj->TryGetStringField(TEXT("op"), Op))
		{
			Out.Op = Op;
		}

		// value 可能是 bool / number / string，统一字符串化
		if (const TSharedPtr<FJsonValue>* ValuePtr = Obj->Values.Find(TEXT("value")))
		{
			Out.Value = UTADialogueSubsystem::JsonScalarToString(*ValuePtr);
		}

		Out.Count = Obj->GetIntegerField(TEXT("count"));
		return true;
	}

	bool ParsePortraitJson(const TSharedPtr<FJsonObject>& Obj, FTAPortraitEntry& Out)
	{
		Out.CharacterId = Obj->GetStringField(TEXT("characterId"));
		if (Out.CharacterId.IsEmpty())
		{
			return false;
		}

		Out.Base = Obj->GetStringField(TEXT("base"));
		Out.EyesOpen = Obj->GetStringField(TEXT("eyesOpen"));
		Out.EyesClosed = Obj->GetStringField(TEXT("eyesClosed"));
		Out.MouthOpen = Obj->GetStringField(TEXT("mouthOpen"));
		Out.MouthClosed = Obj->GetStringField(TEXT("mouthClosed"));

		const TSharedPtr<FJsonObject>* PosObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("position"), PosObj))
		{
			Out.Position.X = (float)(*PosObj)->GetNumberField(TEXT("x"));
			Out.Position.Y = (float)(*PosObj)->GetNumberField(TEXT("y"));
			Out.bPositionSpecified = true;
		}

		double Scale = 0.0;
		if (Obj->TryGetNumberField(TEXT("scale"), Scale))
		{
			Out.Scale = (float)Scale;
			Out.bScaleSpecified = true;
		}

		bool bVisible = true;
		if (Obj->TryGetBoolField(TEXT("visible"), bVisible))
		{
			Out.bVisible = bVisible;
			Out.bVisibleSpecified = true;
		}

		return true;
	}

	bool ParseChoiceJson(const TSharedPtr<FJsonObject>& Obj, FTAStoryChoice& Out)
	{
		Out.TextId = Obj->GetStringField(TEXT("textId"));
		Out.Target = Obj->GetStringField(TEXT("target"));

		const TArray<TSharedPtr<FJsonValue>>* Conds = nullptr;
		if (Obj->TryGetArrayField(TEXT("conditions"), Conds))
		{
			for (const TSharedPtr<FJsonValue>& CondValue : *Conds)
			{
				const TSharedPtr<FJsonObject> CondObj = CondValue->AsObject();
				if (!CondObj.IsValid())
				{
					continue;
				}
				FTAStoryCondition Cond;
				if (ParseConditionJson(CondObj, Cond))
				{
					Out.Conditions.Add(MoveTemp(Cond));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Evts = nullptr;
		if (Obj->TryGetArrayField(TEXT("events"), Evts))
		{
			for (const TSharedPtr<FJsonValue>& EvtValue : *Evts)
			{
				const TSharedPtr<FJsonObject> EvtObj = EvtValue->AsObject();
				if (!EvtObj.IsValid())
				{
					continue;
				}
				FTAStoryEvent Evt;
				if (ParseEventJson(EvtObj, Evt))
				{
					Out.Events.Add(MoveTemp(Evt));
				}
			}
		}

		return !Out.TextId.IsEmpty();
	}

	bool ParseNodeJson(const TSharedPtr<FJsonObject>& Obj, FTAStoryNode& Out)
	{
		Out.Id = Obj->GetStringField(TEXT("id"));
		if (Out.Id.IsEmpty())
		{
			return false;
		}

		FString Type;
		if (Obj->TryGetStringField(TEXT("type"), Type))
		{
			Out.Type = Type;
		}

		Out.SpeakerId = Obj->GetStringField(TEXT("speakerId"));
		Out.SpeakerNameId = Obj->GetStringField(TEXT("speakerNameId"));
		Out.TextId = Obj->GetStringField(TEXT("textId"));
		Out.Next = Obj->GetStringField(TEXT("next"));

		const TArray<TSharedPtr<FJsonValue>>* Portraits = nullptr;
		if (Obj->TryGetArrayField(TEXT("portraits"), Portraits))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Portraits)
			{
				const TSharedPtr<FJsonObject> PObj = Value->AsObject();
				if (!PObj.IsValid())
				{
					continue;
				}
				FTAPortraitEntry Entry;
				if (ParsePortraitJson(PObj, Entry))
				{
					Out.Portraits.Add(MoveTemp(Entry));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* EvtsEnter = nullptr;
		if (Obj->TryGetArrayField(TEXT("eventsOnEnter"), EvtsEnter))
		{
			for (const TSharedPtr<FJsonValue>& Value : *EvtsEnter)
			{
				const TSharedPtr<FJsonObject> EObj = Value->AsObject();
				FTAStoryEvent Evt;
				if (EObj.IsValid() && ParseEventJson(EObj, Evt))
				{
					Out.EventsOnEnter.Add(MoveTemp(Evt));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* EvtsExit = nullptr;
		if (Obj->TryGetArrayField(TEXT("eventsOnExit"), EvtsExit))
		{
			for (const TSharedPtr<FJsonValue>& Value : *EvtsExit)
			{
				const TSharedPtr<FJsonObject> EObj = Value->AsObject();
				FTAStoryEvent Evt;
				if (EObj.IsValid() && ParseEventJson(EObj, Evt))
				{
					Out.EventsOnExit.Add(MoveTemp(Evt));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Obj->TryGetArrayField(TEXT("choices"), Choices))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Choices)
			{
				const TSharedPtr<FJsonObject> CObj = Value->AsObject();
				FTAStoryChoice Choice;
				if (CObj.IsValid() && ParseChoiceJson(CObj, Choice))
				{
					Out.Choices.Add(MoveTemp(Choice));
				}
			}
		}

		return true;
	}
}

bool UTADialogueSubsystem::ParseStoryJson(const FString& JsonString, const FString& SourceName, FTAStoryData& OutData, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON 格式错误");
		return false;
	}

	OutData.StoryId = Root->GetStringField(TEXT("storyId"));
	if (OutData.StoryId.IsEmpty())
	{
		OutData.StoryId = SourceName;
	}

	OutData.Entry = Root->GetStringField(TEXT("entry"));
	if (OutData.Entry.IsEmpty())
	{
		OutError = TEXT("缺少 entry 字段");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	if (!Root->TryGetArrayField(TEXT("nodes"), Nodes) || Nodes->Num() == 0)
	{
		OutError = TEXT("缺少 nodes 字段或为空");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Nodes)
	{
		const TSharedPtr<FJsonObject> NodeObj = Value->AsObject();
		if (!NodeObj.IsValid())
		{
			continue;
		}

		FTAStoryNode Node;
		if (!ParseNodeJson(NodeObj, Node))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Dialogue] %s: 跳过无效节点"), *SourceName);
			continue;
		}

		if (OutData.IndexById.Contains(Node.Id))
		{
			UE_LOG(LogTemp, Error, TEXT("[Dialogue] %s: 节点 ID 重复 %s，已跳过"), *SourceName, *Node.Id);
			continue;
		}

		OutData.IndexById.Add(Node.Id, OutData.Nodes.Num());
		OutData.Nodes.Add(MoveTemp(Node));
	}

	if (OutData.Nodes.Num() == 0)
	{
		OutError = TEXT("没有有效节点");
		return false;
	}

	if (OutData.FindNodeIndex(OutData.Entry) == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("入口节点 %s 不存在"), *OutData.Entry);
		return false;
	}

	return true;
}

// ------------------------------------------------------------
// JSON 序列化（编辑器保存用）
// ------------------------------------------------------------

bool UTADialogueSubsystem::StoryToJson(const FTAStoryData& Story, FString& OutJson)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("storyId"), Story.StoryId);
	Root->SetStringField(TEXT("entry"), Story.Entry);

	auto EventToJson = [](const FTAStoryEvent& Event) -> TSharedPtr<FJsonValue>
	{
		TSharedRef<FJsonObject> EObj = MakeShared<FJsonObject>();
		EObj->SetStringField(TEXT("name"), Event.Name);
		if (Event.Params.Num() > 0)
		{
			TSharedRef<FJsonObject> PObj = MakeShared<FJsonObject>();
			for (const auto& Pair : Event.Params)
			{
				PObj->SetStringField(Pair.Key, Pair.Value);
			}
			EObj->SetObjectField(TEXT("params"), PObj);
		}
		return MakeShared<FJsonValueObject>(EObj);
	};

	TArray<TSharedPtr<FJsonValue>> NodeArray;
	for (const FTAStoryNode& Node : Story.Nodes)
	{
		TSharedRef<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("id"), Node.Id);
		NodeObj->SetStringField(TEXT("type"), Node.Type);

		if (!Node.SpeakerId.IsEmpty())     NodeObj->SetStringField(TEXT("speakerId"), Node.SpeakerId);
		if (!Node.SpeakerNameId.IsEmpty()) NodeObj->SetStringField(TEXT("speakerNameId"), Node.SpeakerNameId);
		if (!Node.TextId.IsEmpty())        NodeObj->SetStringField(TEXT("textId"), Node.TextId);
		if (!Node.Next.IsEmpty())          NodeObj->SetStringField(TEXT("next"), Node.Next);

		if (Node.Portraits.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> PortraitArray;
			for (const FTAPortraitEntry& Entry : Node.Portraits)
			{
				TSharedRef<FJsonObject> PObj = MakeShared<FJsonObject>();
				PObj->SetStringField(TEXT("characterId"), Entry.CharacterId);
				if (!Entry.Base.IsEmpty())        PObj->SetStringField(TEXT("base"), Entry.Base);
				if (!Entry.EyesOpen.IsEmpty())    PObj->SetStringField(TEXT("eyesOpen"), Entry.EyesOpen);
				if (!Entry.EyesClosed.IsEmpty())  PObj->SetStringField(TEXT("eyesClosed"), Entry.EyesClosed);
				if (!Entry.MouthOpen.IsEmpty())   PObj->SetStringField(TEXT("mouthOpen"), Entry.MouthOpen);
				if (!Entry.MouthClosed.IsEmpty()) PObj->SetStringField(TEXT("mouthClosed"), Entry.MouthClosed);

				if (Entry.bPositionSpecified)
				{
					TSharedRef<FJsonObject> PosObj = MakeShared<FJsonObject>();
					PosObj->SetNumberField(TEXT("x"), Entry.Position.X);
					PosObj->SetNumberField(TEXT("y"), Entry.Position.Y);
					PObj->SetObjectField(TEXT("position"), PosObj);
				}
				if (Entry.bScaleSpecified)
				{
					PObj->SetNumberField(TEXT("scale"), Entry.Scale);
				}
				if (Entry.bVisibleSpecified)
				{
					PObj->SetBoolField(TEXT("visible"), Entry.bVisible);
				}
				PortraitArray.Add(MakeShared<FJsonValueObject>(PObj));
			}
			NodeObj->SetArrayField(TEXT("portraits"), PortraitArray);
		}

		if (Node.EventsOnEnter.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FTAStoryEvent& Event : Node.EventsOnEnter) Arr.Add(EventToJson(Event));
			NodeObj->SetArrayField(TEXT("eventsOnEnter"), Arr);
		}
		if (Node.EventsOnExit.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FTAStoryEvent& Event : Node.EventsOnExit) Arr.Add(EventToJson(Event));
			NodeObj->SetArrayField(TEXT("eventsOnExit"), Arr);
		}

		if (Node.Choices.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChoiceArray;
			for (const FTAStoryChoice& Choice : Node.Choices)
			{
				TSharedRef<FJsonObject> CObj = MakeShared<FJsonObject>();
				CObj->SetStringField(TEXT("textId"), Choice.TextId);
				if (!Choice.Target.IsEmpty()) CObj->SetStringField(TEXT("target"), Choice.Target);

				if (Choice.Conditions.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> CondArray;
					for (const FTAStoryCondition& Cond : Choice.Conditions)
					{
						TSharedRef<FJsonObject> CondObj = MakeShared<FJsonObject>();
						CondObj->SetStringField(TEXT("type"), Cond.Type);
						if (!Cond.Flag.IsEmpty())      CondObj->SetStringField(TEXT("flag"), Cond.Flag);
						if (!Cond.Item.IsEmpty())      CondObj->SetStringField(TEXT("item"), Cond.Item);
						if (!Cond.Attribute.IsEmpty()) CondObj->SetStringField(TEXT("attribute"), Cond.Attribute);
						CondObj->SetStringField(TEXT("op"), Cond.Op);
						CondObj->SetStringField(TEXT("value"), Cond.Value);
						CondObj->SetNumberField(TEXT("count"), Cond.Count);
						CondArray.Add(MakeShared<FJsonValueObject>(CondObj));
					}
					CObj->SetArrayField(TEXT("conditions"), CondArray);
				}

				if (Choice.Events.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> Arr;
					for (const FTAStoryEvent& Event : Choice.Events) Arr.Add(EventToJson(Event));
					CObj->SetArrayField(TEXT("events"), Arr);
				}

				ChoiceArray.Add(MakeShared<FJsonValueObject>(CObj));
			}
			NodeObj->SetArrayField(TEXT("choices"), ChoiceArray);
		}

		NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}
	Root->SetArrayField(TEXT("nodes"), NodeArray);

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer, true);
}

// ------------------------------------------------------------
// 对话会话
// ------------------------------------------------------------

bool UTADialogueSubsystem::StartDialogue(const FString& StoryId, UObject* Initiator)
{
	const FTAStoryData* Story = LoadStory(StoryId);
	if (!Story)
	{
		return false;
	}

	if (ActiveController)
	{
		StopDialogue();
	}

	UWorld* World = Initiator ? Initiator->GetWorld() : GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] StartDialogue: 找不到 PlayerController"));
		return false;
	}

	if (!DialogueWidgetClass.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 未配置 DialogueWidgetClass（DefaultGame.ini [/Script/The_Awakening.TADialogueSubsystem]）"));
		return false;
	}

	UTADialogueController* Controller = NewObject<UTADialogueController>(this);
	ActiveController = Controller;
	Controller->Initialize(this, *Story, Initiator);

	UTADialogueWidget* Widget = CreateWidget<UTADialogueWidget>(PC, DialogueWidgetClass.LoadSynchronous());
	if (!Widget)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 创建对话 UI 失败"));
		ActiveController = nullptr;
		return false;
	}

	ActiveWidget = Widget;
	Widget->Setup(this, Controller);
	Widget->AddToViewport(100);

	Controller->Start();
	return true;
}

void UTADialogueSubsystem::StopDialogue()
{
	if (!ActiveController)
	{
		return;
	}

	// 先关 UI（RemoveFromParent 触发 NativeDestruct 解绑输入）
	if (ActiveWidget)
	{
		ActiveWidget->CloseDialogue();
		ActiveWidget = nullptr;
	}

	ActiveController->Shutdown();
	ActiveController = nullptr;

	OnDialogueEnded.Broadcast();
}

// ------------------------------------------------------------
// 存档接口
// ------------------------------------------------------------

FTAStorySaveData UTADialogueSubsystem::GetSaveData() const
{
	FTAStorySaveData Save;
	Save.Flags = Flags;
	Save.StoryProgress = StoryProgress;
	Save.CompletedStories = CompletedStories;
	return Save;
}

void UTADialogueSubsystem::RestoreSaveData(const FTAStorySaveData& SaveData)
{
	Flags = SaveData.Flags;
	StoryProgress = SaveData.StoryProgress;
	CompletedStories = SaveData.CompletedStories;
}

void UTADialogueSubsystem::UpdateStoryProgress(const FString& StoryId, const FString& NodeId)
{
	if (!StoryId.IsEmpty())
	{
		StoryProgress.Add(StoryId, NodeId);
	}
}

void UTADialogueSubsystem::MarkStoryCompleted(const FString& StoryId)
{
	if (!StoryId.IsEmpty())
	{
		CompletedStories.AddUnique(StoryId);
	}
}
