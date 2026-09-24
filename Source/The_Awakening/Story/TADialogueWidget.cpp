// Source/The_Awakening/Story/TADialogueWidget.cpp
#include "Story/TADialogueWidget.h"
#include "Story/TADialogueController.h"
#include "Story/TADialogueSubsystem.h"
#include "Story/TAPortraitWidget.h"
#include "Story/TADialogueChoiceButton.h"
#include "Story/TADialogueHistoryWidget.h"
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
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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

void UTADialogueWidget::Setup(UTADialogueSubsystem* InSubsystem, UTADialogueController* InController)
{
	Subsystem = InSubsystem;
	Controller = InController;
}

void UTADialogueWidget::EnsureBindings()
{
	if (!Text_Name) Text_Name = FindDialogueWidget<UTextBlock>(this, TEXT("Text_Name"));
	if (!Text_Dialogue) Text_Dialogue = FindDialogueWidget<UTextBlock>(this, TEXT("Text_Dialogue"));
	if (!Button_Continue) Button_Continue = FindDialogueWidget<UButton>(this, TEXT("Button_Continue"));
	if (!Button_History) Button_History = FindDialogueWidget<UButton>(this, TEXT("Button_History"));
	if (!Root) Root = FindDialogueWidget<UCanvasPanel>(this, TEXT("Root"));
	if (!Box_Choices) Box_Choices = FindDialogueWidget<UVerticalBox>(this, TEXT("Box_Choices"));
	if (!Panel_Choices) Panel_Choices = FindDialogueWidget<UPanelWidget>(this, TEXT("Panel_Choices"));
	if (!Widget_History) Widget_History = FindDialogueWidget<UTADialogueHistoryWidget>(this, TEXT("Widget_History"));
	if (!Image_ContinueIcon) Image_ContinueIcon = FindDialogueWidget<UImage>(this, TEXT("Image_ContinueIcon"));
	if (!Image_HistoryIcon) Image_HistoryIcon = FindDialogueWidget<UImage>(this, TEXT("Image_HistoryIcon"));
	EnsurePortraitLayer();
}

bool UTADialogueWidget::EnsurePortraitLayer()
{
	if (RuntimePortraitCanvas)
	{
		return true;
	}
	UCanvasPanel* RootCanvas = Root;
	if (!RootCanvas)
	{
		RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	}
	if (!RootCanvas)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] 无法绑定 WBP_Dialogue 根 Canvas Panel（层级显示 Canvas Panel_Root，控件名应为 Root）"));
		return false;
	}

	RuntimePortraitCanvas = WidgetTree
		? WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RuntimePortraitCanvas"))
		: NewObject<UCanvasPanel>(this, TEXT("RuntimePortraitCanvas"));
	if (!RuntimePortraitCanvas)
	{
		return false;
	}

	if (UCanvasPanelSlot* CanvasChildSlot = RootCanvas->AddChildToCanvas(RuntimePortraitCanvas))
	{
		CanvasChildSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasChildSlot->SetOffsets(FMargin(0.f));
		CanvasChildSlot->SetZOrder(100);
		return true;
	}

	RuntimePortraitCanvas = nullptr;
	UE_LOG(LogTemp, Error, TEXT("[Dialogue] 无法将运行时立绘层添加到根控件 Root"));
	return false;
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

	// 按钮
	if (Button_Continue)
	{
		Button_Continue->OnClicked.AddDynamic(this, &UTADialogueWidget::OnAdvancePressed);
	}
	if (Button_History)
	{
		Button_History->OnClicked.AddDynamic(this, &UTADialogueWidget::ToggleHistory);
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

	// 初始状态
	RefreshLine();
	RefreshChoices();
	RefreshPortraits();
	RefreshIcons();
}

void UTADialogueWidget::NativeDestruct()
{
	PopDialogueMappingContext();
	UnbindInputActions();

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

	if (Controller)
	{
		Controller->Tick(InDeltaTime);

		// 打字机逐帧刷新可见文本
		if (Controller->IsTyping() && Text_Dialogue)
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
}

void UTADialogueWidget::OnAdvancePressed()
{
	if (Controller)
	{
		Controller->Advance();
	}
}

void UTADialogueWidget::ToggleHistory()
{
	if (!Widget_History)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] WBP_Dialogue 缺少 Widget_History（WBP_DialogueHistory 实例）"));
		return;
	}

	bHistoryOpen = !bHistoryOpen;
	Widget_History->SetVisibility(bHistoryOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bHistoryOpen)
	{
		RefreshHistory();
	}
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

	if (Button_Continue)
	{
		Button_Continue->SetVisibility(bChoice ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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

	if (!Controller->IsChoiceNode() || !Box_Choices || !Subsystem)
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
		Box_Choices->AddChildToVerticalBox(ChoiceW);
		ChoiceWidgets.Add(ChoiceW);
	}
}

void UTADialogueWidget::RefreshPortraits()
{
	if (!Controller || !Subsystem)
	{
		return;
	}
	if (!EnsurePortraitLayer())
	{
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
			UTAPortraitWidget* NewPortrait = CreateWidget<UTAPortraitWidget>(this, Subsystem->PortraitWidgetClass.LoadSynchronous());
			if (!NewPortrait)
			{
				continue;
			}
			RuntimePortraitCanvas->AddChildToCanvas(NewPortrait);
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
	if (Widget_History && bHistoryOpen && Controller)
	{
		Widget_History->SetHistory(Controller->GetHistory());
	}
}

void UTADialogueWidget::RefreshIcons()
{
	if (!InputIconSubsystem)
	{
		return;
	}

	if (Image_ContinueIcon)
	{
		Image_ContinueIcon->SetBrushFromTexture(AdvanceAction ? InputIconSubsystem->GetIconForAction(AdvanceAction) : nullptr);
	}
	if (Image_HistoryIcon)
	{
		Image_HistoryIcon->SetBrushFromTexture(HistoryAction ? InputIconSubsystem->GetIconForAction(HistoryAction) : nullptr);
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
}

void UTADialogueWidget::HandleInputDeviceChanged()
{
	RefreshIcons();
}
