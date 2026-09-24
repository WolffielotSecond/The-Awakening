// Source/The_Awakening/Story/TADialogueWidget.cpp
#include "Story/TADialogueWidget.h"
#include "Story/TADialogueController.h"
#include "Story/TADialogueSubsystem.h"
#include "Story/TADialoguePortraitLayerWidget.h"
#include "Story/TAPortraitWidget.h"
#include "Story/TADialogueChoiceButton.h"
#include "Story/TADialogueHistoryWidget.h"
#include "UI/TAActionPromptWidget.h"
#include "The_AwakeningCharacter.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "The_AwakeningPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	template <typename WidgetType>
	WidgetType* FindDialogueWidget(UUserWidget* Owner, const TCHAR* WidgetName)
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

void UTADialogueWidget::Setup(UTADialogueSubsystem* InSubsystem, UTADialogueController* InController, UTADialoguePortraitLayerWidget* InPortraitLayer)
{
	Subsystem = InSubsystem;
	Controller = InController;
	PortraitLayer = InPortraitLayer;
}

void UTADialogueWidget::EnsureBindings()
{
	if (!Text_Name) Text_Name = FindDialogueWidget<UTextBlock>(this, TEXT("Text_Name"));
	if (!Text_Dialogue) Text_Dialogue = FindDialogueWidget<UTextBlock>(this, TEXT("Text_Dialogue"));
	if (!Button_Continue) Button_Continue = FindDialogueWidget<UButton>(this, TEXT("Button_Continue"));
	if (!Button_History) Button_History = FindDialogueWidget<UButton>(this, TEXT("Button_History"));
	if (!Box_Choices) Box_Choices = FindDialogueWidget<UVerticalBox>(this, TEXT("Box_Choices"));
	if (!Panel_Choices) Panel_Choices = FindDialogueWidget<UPanelWidget>(this, TEXT("Panel_Choices"));
	if (!LegacyEmbeddedHistoryWidget) LegacyEmbeddedHistoryWidget = FindDialogueWidget<UWidget>(this, TEXT("Widget_History"));
	if (!LegacyEmbeddedHistoryWidget) LegacyEmbeddedHistoryWidget = FindDialogueWidget<UWidget>(this, TEXT("WBP_DialogueHistory"));
	if (!Image_ContinueIcon) Image_ContinueIcon = FindDialogueWidget<UImage>(this, TEXT("Image_ContinueIcon"));
	if (!Image_HistoryIcon) Image_HistoryIcon = FindDialogueWidget<UImage>(this, TEXT("Image_HistoryIcon"));
	if (!Text_ContinueText) Text_ContinueText = FindDialogueWidget<UTextBlock>(this, TEXT("Text_ContinueText"));
	if (!Text_HistoryText) Text_HistoryText = FindDialogueWidget<UTextBlock>(this, TEXT("Text_HistoryText"));
	if (!HorizontalBox_Controls) HorizontalBox_Controls = FindDialogueWidget<UHorizontalBox>(this, TEXT("HorizontalBox_Controls"));

	bHistoryOpen = false;
	if (LegacyEmbeddedHistoryWidget)
	{
		LegacyEmbeddedHistoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTADialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureBindings();

	// 子系统（本地化 / 输入图标）
	if (UGameInstance* GI = GetGameInstance())
	{
		LocalizeSubsystem = GI->GetSubsystem<UTALocalizeSubsystem>();
		InputIconSubsystem = GI->GetSubsystem<UTAInputIconSubsystem>();
	}

	// 子系统委托
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.AddDynamic(this, &UTADialogueWidget::HandleLanguageChanged);
	}
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.AddDynamic(this, &UTADialogueWidget::HandleInputDeviceChanged);
	}
	EnsureChoiceInputActions();
	BuildActionPromptBar();

	// 控制器委托
	if (Controller)
	{
		Controller->OnLineShown.AddDynamic(this, &UTADialogueWidget::HandleLineShown);
		Controller->OnChoicesChanged.AddDynamic(this, &UTADialogueWidget::HandleChoicesChanged);
		Controller->OnPortraitsChanged.AddDynamic(this, &UTADialogueWidget::HandlePortraitsChanged);
		Controller->OnSpeakerChanged.AddDynamic(this, &UTADialogueWidget::HandleSpeakerChanged);
		Controller->OnHistoryChanged.AddDynamic(this, &UTADialogueWidget::HandleHistoryChanged);
		Controller->OnStoryFinished.AddDynamic(this, &UTADialogueWidget::HandleStoryFinished);
	}

	// 输入
	BindInputActions();
	PushDialogueMappingContext();
	if (InputIconSubsystem)
	{
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			InputIconSubsystem->RefreshCurrentDeviceForUser(LocalPlayer->GetPlatformUserId());
		}
	}

	// 初始状态
	RefreshLine();
	RefreshChoices();
	RefreshPortraits();
	RefreshIcons();
	bRefreshIconsOnNextTick = true;
	RefreshPromptLabels();
}

void UTADialogueWidget::NativeDestruct()
{
	PopDialogueMappingContext();
	UnbindInputActions();
	if (ActiveHistoryWidget)
	{
		ActiveHistoryWidget->RemoveFromParent();
		ActiveHistoryWidget = nullptr;
	}
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.RemoveDynamic(this, &UTADialogueWidget::HandleLanguageChanged);
		LocalizeSubsystem = nullptr;
	}
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.RemoveDynamic(this, &UTADialogueWidget::HandleInputDeviceChanged);
		InputIconSubsystem = nullptr;
	}
	if (Controller)
	{
		Controller->OnLineShown.RemoveDynamic(this, &UTADialogueWidget::HandleLineShown);
		Controller->OnChoicesChanged.RemoveDynamic(this, &UTADialogueWidget::HandleChoicesChanged);
		Controller->OnPortraitsChanged.RemoveDynamic(this, &UTADialogueWidget::HandlePortraitsChanged);
		Controller->OnSpeakerChanged.RemoveDynamic(this, &UTADialogueWidget::HandleSpeakerChanged);
		Controller->OnHistoryChanged.RemoveDynamic(this, &UTADialogueWidget::HandleHistoryChanged);
		Controller->OnStoryFinished.RemoveDynamic(this, &UTADialogueWidget::HandleStoryFinished);
	}

	Super::NativeDestruct();
}

void UTADialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bRefreshIconsOnNextTick)
	{
		bRefreshIconsOnNextTick = false;
		RefreshIcons();
	}

	if (Controller)
	{
		Controller->Tick(InDeltaTime);
		UpdateChoiceSelectionFromMouse();

		// Refresh after Tick even when it just completed the line. Tick may flip
		// IsTyping to false on the exact frame that reveals the final character.
		if (Text_Dialogue && !Controller->IsChoiceNode())
		{
			Text_Dialogue->SetText(Controller->GetVisibleText());
		}

		UpdateTalkingFlags();
	}
}

void UTADialogueWidget::CloseDialogue()
{
	PopDialogueMappingContext();
	RemoveFromParent();
}

// ------------------------------------------------------------
// 输入
// ------------------------------------------------------------

void UTADialogueWidget::BindInputActions()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->InputComponent)
	{
		return;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!EIC)
	{
		return;
	}

	if (AdvanceAction)
	{
		InputBindingHandles.Add(EIC->BindAction(AdvanceAction, ETriggerEvent::Started, this, &UTADialogueWidget::OnAdvancePressed).GetHandle());
	}
	if (HistoryAction)
	{
		InputBindingHandles.Add(EIC->BindAction(HistoryAction, ETriggerEvent::Started, this, &UTADialogueWidget::ToggleHistory).GetHandle());
	}
	if (ChoicePreviousAction)
	{
		InputBindingHandles.Add(EIC->BindAction(ChoicePreviousAction, ETriggerEvent::Started, this, &UTADialogueWidget::OnChoicePreviousPressed).GetHandle());
	}
	if (ChoiceNextAction)
	{
		InputBindingHandles.Add(EIC->BindAction(ChoiceNextAction, ETriggerEvent::Started, this, &UTADialogueWidget::OnChoiceNextPressed).GetHandle());
	}
	if (ChoiceConfirmAction)
	{
		InputBindingHandles.Add(EIC->BindAction(ChoiceConfirmAction, ETriggerEvent::Started, this, &UTADialogueWidget::OnChoiceConfirmPressed).GetHandle());
	}

}

void UTADialogueWidget::EnsureChoiceInputActions()
{
	if (!ChoicePreviousAction)
	{
		ChoicePreviousAction = NewObject<UInputAction>(this, TEXT("Runtime_DialogueChoicePrevious"));
	}
	if (!ChoiceNextAction)
	{
		ChoiceNextAction = NewObject<UInputAction>(this, TEXT("Runtime_DialogueChoiceNext"));
	}
	if (!ChoiceConfirmAction)
	{
		ChoiceConfirmAction = NewObject<UInputAction>(this, TEXT("Runtime_DialogueChoiceConfirm"));
	}

	RuntimeChoiceMappingContext = NewObject<UInputMappingContext>(this, TEXT("Runtime_DialogueChoiceMappingContext"));
	if (!RuntimeChoiceMappingContext)
	{
		return;
	}

	// Only supply defaults for transiently-created actions. Designer-assigned actions are expected
	// to be mapped in DialogueMappingContext so project input settings remain authoritative.
	if (ChoicePreviousAction && ChoicePreviousAction->GetFName() == TEXT("Runtime_DialogueChoicePrevious"))
	{
		RuntimeChoiceMappingContext->MapKey(ChoicePreviousAction, EKeys::Up);
		RuntimeChoiceMappingContext->MapKey(ChoicePreviousAction, EKeys::Gamepad_DPad_Up);
	}
	if (ChoiceNextAction && ChoiceNextAction->GetFName() == TEXT("Runtime_DialogueChoiceNext"))
	{
		RuntimeChoiceMappingContext->MapKey(ChoiceNextAction, EKeys::Down);
		RuntimeChoiceMappingContext->MapKey(ChoiceNextAction, EKeys::Gamepad_DPad_Down);
	}
	if (ChoiceConfirmAction && ChoiceConfirmAction->GetFName() == TEXT("Runtime_DialogueChoiceConfirm"))
	{
		RuntimeChoiceMappingContext->MapKey(ChoiceConfirmAction, EKeys::Enter);
		RuntimeChoiceMappingContext->MapKey(ChoiceConfirmAction, EKeys::SpaceBar);
		RuntimeChoiceMappingContext->MapKey(ChoiceConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
	}
}

void UTADialogueWidget::OnChoicePreviousPressed()
{
	if (Controller && !bHistoryOpen && Controller->IsChoiceNode())
	{
		Controller->MoveSelection(-1);
		UpdateChoiceHighlights();
	}
}

void UTADialogueWidget::OnChoiceNextPressed()
{
	if (Controller && !bHistoryOpen && Controller->IsChoiceNode())
	{
		Controller->MoveSelection(1);
		UpdateChoiceHighlights();
	}
}

void UTADialogueWidget::OnChoiceConfirmPressed()
{
	if (!Controller || bHistoryOpen || !Controller->IsChoiceNode())
	{
		return;
	}

	if (ChoiceConfirmPressedFrame == GFrameCounter)
	{
		return;
	}
	ChoiceConfirmPressedFrame = GFrameCounter;
	Controller->SelectChoice(Controller->GetSelectedChoiceIndex());
}

void UTADialogueWidget::OnActionPromptClicked(UTAActionPromptWidget* Prompt)
{
	if (!Prompt || !Prompt->GetPromptAction())
	{
		return;
	}

	UInputAction* Action = Prompt->GetPromptAction();
	if (Controller && Controller->IsChoiceNode())
	{
		if (Action == ChoicePreviousAction)
		{
			OnChoicePreviousPressed();
			return;
		}
		if (Action == ChoiceNextAction)
		{
			OnChoiceNextPressed();
			return;
		}
		if (Action == ChoiceConfirmAction)
		{
			OnChoiceConfirmPressed();
			return;
		}
	}

	if (Action == HistoryAction)
	{
		ToggleHistory();
	}
	else if (Action == AdvanceAction)
	{
		OnAdvancePressed();
	}
}

void UTADialogueWidget::UnbindInputActions()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->InputComponent)
	{
		InputBindingHandles.Reset();
		return;
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		for (uint32 Handle : InputBindingHandles)
		{
			EIC->RemoveBindingByHandle(Handle);
		}
	}
	InputBindingHandles.Reset();
}

void UTADialogueWidget::PushDialogueMappingContext()
{
	if (bMappingPushed)
	{
		return;
	}

	if (DialogueMappingContext)
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* EILP = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				EILP->AddMappingContext(DialogueMappingContext, DialogueMappingPriority);
				if (RuntimeChoiceMappingContext)
				{
					EILP->AddMappingContext(RuntimeChoiceMappingContext, DialogueMappingPriority + 1);
				}
			}
		}
	}
	else if (RuntimeChoiceMappingContext)
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* EILP = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				EILP->AddMappingContext(RuntimeChoiceMappingContext, DialogueMappingPriority + 1);
			}
		}
	}

	// 移除默认移动等映射（PlayerController 统一处理；编辑器预览无 PC 时跳过）
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AThe_AwakeningPlayerController* TAPC = Cast<AThe_AwakeningPlayerController>(PC))
		{
			TAPC->SetDialogueModeActive(true);
		}
	}

	bMappingPushed = true;
}

void UTADialogueWidget::PopDialogueMappingContext()
{
	if (!bMappingPushed)
	{
		return;
	}
	bMappingPushed = false;

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AThe_AwakeningPlayerController* TAPC = Cast<AThe_AwakeningPlayerController>(PC))
		{
			TAPC->SetDialogueModeActive(false);
		}
	}

	if (DialogueMappingContext)
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* EILP = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				EILP->RemoveMappingContext(DialogueMappingContext);
			}
		}
	}
	if (RuntimeChoiceMappingContext)
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* EILP = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				EILP->RemoveMappingContext(RuntimeChoiceMappingContext);
			}
		}
	}
}

void UTADialogueWidget::OnAdvancePressed()
{
	AdvancePressedFrame = GFrameCounter;
	// Choice confirmation may already have entered the next dialogue node earlier
	// in this frame. Do not let the same physical press immediately advance or
	// complete that newly displayed line.
	if (ChoiceConfirmPressedFrame == GFrameCounter)
	{
		return;
	}

	if (Controller && !bHistoryOpen)
	{
		// Projects may bind the established Advance action to the same confirm
		// button used by dialogue choices. In a choice node, activate the current
		// highlighted choice instead of forwarding an Advance that the controller
		// intentionally ignores for branches.
		if (Controller->IsChoiceNode())
		{
			// Mouse clicks are handled by the specific choice button's OnClicked
			// callback, which carries that button's index. Do not also treat the
			// left mouse button as confirmation of the keyboard/gamepad selection.
			if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
			{
				return;
			}
			OnChoiceConfirmPressed();
			return;
		}
		Controller->Advance();
	}
}

void UTADialogueWidget::ToggleHistory()
{
	// If the same physical key is mapped to both actions, Advance takes precedence.
	// This also prevents a focused history button from reopening the overlay on that press.
	if (AdvancePressedFrame == GFrameCounter)
	{
		return;
	}

	if (ActiveHistoryWidget && ActiveHistoryWidget->IsInViewport())
	{
		CloseHistoryOverlay();
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	AThe_AwakeningCharacter* PlayerCharacter = PC ? Cast<AThe_AwakeningCharacter>(PC->GetPawn()) : nullptr;
	UClass* HistoryClass = PlayerCharacter ? PlayerCharacter->GetDialogueHistoryWidgetClass().Get() : nullptr;
	if (!HistoryClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 未配置 DialogueHistoryWidgetClass：请在玩家角色蓝图的 Details > UI 中指定独立的 WBP_DialogueHistory"));
		return;
	}

	ActiveHistoryWidget = CreateWidget<UTADialogueHistoryWidget>(PC, HistoryClass);
	if (!ActiveHistoryWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 创建独立历史 UI 失败"));
		return;
	}

	bHistoryOpen = true;
	ActiveHistoryWidget->SetupClosePrompt(HistoryAction);
	ActiveHistoryWidget->OnCloseRequested.AddDynamic(this, &UTADialogueWidget::CloseHistoryOverlay);
	ActiveHistoryWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ActiveHistoryWidget->SetHistory(Controller ? Controller->GetHistory() : TArray<FTAStoryHistoryEntry>());
	ActiveHistoryWidget->AddToViewport(200);
	RefreshActionPromptBar();
	if (AThe_AwakeningPlayerController* TAPC = Cast<AThe_AwakeningPlayerController>(PC))
	{
		TAPC->SetUIFocusWidget(ActiveHistoryWidget);
	}
	if (Button_Continue)
	{
		ContinueButtonVisibilityBeforeHistory = Button_Continue->GetVisibility();
		bContinueButtonWasEnabledBeforeHistory = Button_Continue->GetIsEnabled();
		Button_Continue->SetIsEnabled(false);
		Button_Continue->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_History)
	{
		HistoryButtonVisibilityBeforeHistory = Button_History->GetVisibility();
		Button_History->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTADialogueWidget::CloseHistoryOverlay()
{
	if (ActiveHistoryWidget)
	{
		ActiveHistoryWidget->RemoveFromParent();
		ActiveHistoryWidget = nullptr;
	}
	bHistoryOpen = false;
	if (Button_Continue)
	{
		Button_Continue->SetIsEnabled(bContinueButtonWasEnabledBeforeHistory);
		Button_Continue->SetVisibility(ContinueButtonVisibilityBeforeHistory);
	}
	if (Button_History)
	{
		Button_History->SetVisibility(HistoryButtonVisibilityBeforeHistory);
	}
	RefreshActionPromptBar();
}

void UTADialogueWidget::OnChoiceClicked(int32 Index)
{
	if (Controller)
	{
		Controller->SelectChoice(Index);
	}
}

// ------------------------------------------------------------
// 渲染刷新
// ------------------------------------------------------------

void UTADialogueWidget::RefreshLine()
{
	if (!Controller)
	{
		return;
	}

	const bool bChoice = Controller->IsChoiceNode();

	if (Text_Name)
	{
		const FText Name = Controller->GetSpeakerName();
		Text_Name->SetText(Name);
		Text_Name->SetVisibility(Name.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Text_Dialogue)
	{
		Text_Dialogue->SetText(bChoice ? FText::GetEmpty() : Controller->GetVisibleText());
	}

}

void UTADialogueWidget::RefreshChoices()
{
	if (!Controller)
	{
		return;
	}

	// 清空旧选项
	for (UTADialogueChoiceButton* Old : ChoiceWidgets)
	{
		if (Old)
		{
			Old->RemoveFromParent();
		}
	}
	ChoiceWidgets.Reset();

	// 面板显隐
	if (Panel_Choices)
	{
		Panel_Choices->SetVisibility(Controller->IsChoiceNode() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	else if (Box_Choices)
	{
		Box_Choices->SetVisibility(Controller->IsChoiceNode() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const bool bIsChoiceNode = Controller->IsChoiceNode();
	RefreshActionPromptBar();

	if (!bIsChoiceNode || !Box_Choices || !Subsystem)
	{
		return;
	}

	if (Subsystem->ChoiceButtonWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 未配置 ChoiceButtonWidgetClass"));
		return;
	}

	const TArray<FText> Texts = Controller->GetVisibleChoiceTexts();
	const int32 Selected = Controller->GetSelectedChoiceIndex();

	for (int32 i = 0; i < Texts.Num(); ++i)
	{
		UTADialogueChoiceButton* ChoiceW = CreateWidget<UTADialogueChoiceButton>(this, Subsystem->ChoiceButtonWidgetClass.LoadSynchronous());
		if (!ChoiceW)
		{
			continue;
		}
		ChoiceW->Setup(i, Texts[i]);
		ChoiceW->SetHighlighted(i == Selected);
		ChoiceW->OnClickedIndex.AddDynamic(this, &UTADialogueWidget::OnChoiceClicked);
		ChoiceW->OnFocusedIndex.AddDynamic(this, &UTADialogueWidget::OnChoiceFocused);
		Box_Choices->AddChildToVerticalBox(ChoiceW);
		ChoiceWidgets.Add(ChoiceW);
	}
	UpdateChoiceHighlights();
}

void UTADialogueWidget::RefreshPortraits()
{
	if (!Controller || !Subsystem)
	{
		return;
	}
	if (!PortraitLayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 未配置 DialoguePortraitLayerClass，无法显示立绘层"));
		return;
	}

	if (Subsystem->PortraitWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 未配置 PortraitWidgetClass"));
		return;
	}

	const TArray<FTAPortraitEntry> Entries = Controller->GetPortraits();

	// 移除不存在的立绘（淡出后自删）
	for (int32 i = PortraitWidgets.Num() - 1; i >= 0; --i)
	{
		UTAPortraitWidget* Portrait = PortraitWidgets[i];
		if (!Portrait)
		{
			PortraitWidgets.RemoveAt(i);
			continue;
		}

		const bool bStillExists = Entries.ContainsByPredicate(
			[&](const FTAPortraitEntry& E) { return E.CharacterId == Portrait->GetCharacterId(); });
		if (!bStillExists)
		{
			Portrait->FadeOutAndRemove();
			PortraitWidgets.RemoveAt(i);
		}
	}

	// 更新/新建
	for (const FTAPortraitEntry& Entry : Entries)
	{
		TObjectPtr<UTAPortraitWidget>* Existing = PortraitWidgets.FindByPredicate(
			[&](const TObjectPtr<UTAPortraitWidget>& P) { return P && P->GetCharacterId() == Entry.CharacterId; });

		if (!Existing)
		{
			UTAPortraitWidget* NewPortrait = CreateWidget<UTAPortraitWidget>(PortraitLayer, Subsystem->PortraitWidgetClass.LoadSynchronous());
			if (!NewPortrait)
			{
				continue;
			}
			if (!PortraitLayer->AddPortraitWidget(NewPortrait))
			{
				NewPortrait->RemoveFromParent();
				continue;
			}
			PortraitWidgets.Add(NewPortrait);
			Existing = &PortraitWidgets.Last();
		}

		(*Existing)->ApplyPortrait(Entry);
	}

	UpdateTalkingFlags();
}

void UTADialogueWidget::UpdateTalkingFlags()
{
	if (!Controller)
	{
		return;
	}

	const bool bTyping = Controller->IsTyping();
	const FString Speaker = Controller->GetSpeakerId();

	for (UTAPortraitWidget* Portrait : PortraitWidgets)
	{
		if (Portrait)
		{
			Portrait->SetTalking(bTyping && !Speaker.IsEmpty() && Portrait->GetCharacterId() == Speaker);
		}
	}
}

void UTADialogueWidget::RefreshHistory()
{
	if (ActiveHistoryWidget && bHistoryOpen && Controller)
	{
		ActiveHistoryWidget->SetHistory(Controller->GetHistory());
	}
}

void UTADialogueWidget::RefreshIcons()
{
	RefreshActionPromptBar();
}

void UTADialogueWidget::RefreshPromptLabels()
{
	RefreshActionPromptBar();
}

void UTADialogueWidget::BuildActionPromptBar()
{
	if (!HorizontalBox_Controls || ActionPromptWidgets.Num() > 0)
	{
		return;
	}
	UClass* PromptClass = ActionPromptWidgetClass.Get();
	if (!PromptClass)
	{
		PromptClass = LoadClass<UTAActionPromptWidget>(nullptr, TEXT("/Game/UI/WBP_ActionPrompt.WBP_ActionPrompt_C"));
	}
	if (!PromptClass)
	{
		PromptClass = UTAActionPromptWidget::StaticClass();
	}
	auto AddPrompt = [this, PromptClass](UInputAction* Action, const TCHAR* TextId) -> UTAActionPromptWidget*
	{
		UTAActionPromptWidget* Prompt = CreateWidget<UTAActionPromptWidget>(this, PromptClass);
		if (!Prompt)
		{
			return nullptr;
		}
		Prompt->ConfigureLocalizedPrompt(Action, TextId);
		Prompt->OnPromptClicked.AddDynamic(this, &UTADialogueWidget::OnActionPromptClicked);
		HorizontalBox_Controls->AddChildToHorizontalBox(Prompt)->SetPadding(FMargin(6.0f, 0.0f));
		ActionPromptWidgets.Add(Prompt);
		return Prompt;
	};

	if (Button_Continue)
	{
		Button_Continue->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Button_History)
	{
		Button_History->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (HorizontalBox_Controls)
	{
		auto AddPromptToGroup = [this, &AddPrompt](UInputAction* Action, const TCHAR* TextId, TArray<TObjectPtr<UTAActionPromptWidget>>& Group)
		{
			if (UTAActionPromptWidget* Prompt = AddPrompt(Action, TextId))
			{
				Group.Add(Prompt);
			}
		};
		AddPromptToGroup(AdvanceAction, TEXT("UI_Dialogue_Continue"), BasePromptWidgets);
		AddPromptToGroup(HistoryAction, TEXT("UI_Dialogue_History"), BasePromptWidgets);
		AddPromptToGroup(ChoicePreviousAction, TEXT("UI_Dialogue_ChoiceUp"), ChoicePromptWidgets);
		AddPromptToGroup(ChoiceNextAction, TEXT("UI_Dialogue_ChoiceDown"), ChoicePromptWidgets);
		AddPromptToGroup(ChoiceConfirmAction, TEXT("UI_Dialogue_ChoiceSelect"), ChoicePromptWidgets);
	}
	RefreshActionPromptBar();
}

void UTADialogueWidget::RefreshActionPromptBar()
{
	if (!HorizontalBox_Controls)
	{
		return;
	}
	const bool bHasDialogue = Controller && !bHistoryOpen;
	const bool bShowChoicePrompts = bHasDialogue && Controller->IsChoiceNode();
	const bool bShowBasePrompts = bHasDialogue && !Controller->IsChoiceNode();
	HorizontalBox_Controls->SetVisibility(bHasDialogue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (int32 Index = 0; Index < BasePromptWidgets.Num(); ++Index)
	{
		if (BasePromptWidgets[Index])
		{
			const bool bVisible = bHasDialogue && (Index == 1 || bShowBasePrompts);
			BasePromptWidgets[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	for (UTAActionPromptWidget* Prompt : ChoicePromptWidgets)
	{
		if (Prompt)
		{
			Prompt->SetVisibility(bShowChoicePrompts ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	for (UTAActionPromptWidget* Prompt : ActionPromptWidgets)
	{
		if (Prompt)
		{
			Prompt->RefreshPrompt();
		}
	}
}

void UTADialogueWidget::UpdateChoiceHighlights()
{
	const int32 Selected = Controller ? Controller->GetSelectedChoiceIndex() : INDEX_NONE;
	for (int32 Index = 0; Index < ChoiceWidgets.Num(); ++Index)
	{
		if (ChoiceWidgets[Index])
		{
			ChoiceWidgets[Index]->SetHighlighted(Index == Selected);
		}
	}
}

void UTADialogueWidget::UpdateChoiceSelectionFromMouse()
{
	if (!Controller || !Controller->IsChoiceNode() || bHistoryOpen || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();
	if (!bHasLastChoiceMousePosition)
	{
		LastChoiceMousePosition = MousePosition;
		bHasLastChoiceMousePosition = true;
		return;
	}

	const bool bMouseMoved = !MousePosition.Equals(LastChoiceMousePosition, 0.5f);
	LastChoiceMousePosition = MousePosition;
	if (!bMouseMoved)
	{
		return;
	}

	for (int32 Index = 0; Index < ChoiceWidgets.Num(); ++Index)
	{
		UTADialogueChoiceButton* Choice = ChoiceWidgets[Index];
		if (Choice && Choice->GetCachedGeometry().IsUnderLocation(MousePosition))
		{
			if (Controller->GetSelectedChoiceIndex() != Index)
			{
				Controller->SetSelectedChoiceIndex(Index);
				UpdateChoiceHighlights();
			}
			return;
		}
	}
}

// ------------------------------------------------------------
// 控制器回调
// ------------------------------------------------------------

void UTADialogueWidget::HandleLineShown()
{
	RefreshLine();
}

void UTADialogueWidget::HandleChoicesChanged()
{
	RefreshChoices();
}

void UTADialogueWidget::OnChoiceFocused(int32 Index)
{
	if (Controller && Controller->IsChoiceNode() && !bHistoryOpen)
	{
		Controller->SetSelectedChoiceIndex(Index);
		UpdateChoiceHighlights();
	}
}

void UTADialogueWidget::HandlePortraitsChanged()
{
	RefreshPortraits();
}

void UTADialogueWidget::HandleSpeakerChanged()
{
	RefreshLine();
	UpdateTalkingFlags();
}

void UTADialogueWidget::HandleHistoryChanged()
{
	RefreshHistory();
}

void UTADialogueWidget::HandleStoryFinished()
{
	// 编辑器预览不接管子系统会话
	if (!bPreviewMode && Subsystem)
	{
		Subsystem->StopDialogue();
	}
}

void UTADialogueWidget::HandleLanguageChanged()
{
	RefreshLine();
	RefreshChoices();
	RefreshHistory();
	RefreshPromptLabels();
}

void UTADialogueWidget::HandleInputDeviceChanged()
{
	RefreshIcons();
}
