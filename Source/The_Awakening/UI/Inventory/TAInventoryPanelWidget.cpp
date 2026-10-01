#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Core/TAPlayerState.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "UI/TAActionPromptWidget.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"

FReply UTAInventoryPanelWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Prevent OS/gamepad key repeat from reaching Blueprint close handlers after opening the panel.
	if (InKeyEvent.IsRepeat() && (InKeyEvent.GetKey() == EKeys::Tab || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Left))
		return FReply::Handled();
	if (InKeyEvent.GetKey() != EKeys::Gamepad_FaceButton_Bottom)
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	if (!InKeyEvent.IsRepeat() && FSlateApplication::IsInitialized())
	{
		const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
		auto IsUnderCursor = [&CursorPosition](UButton* Button)
		{
			return Button && Button->IsVisible() && Button->GetIsEnabled() && Button->GetCachedGeometry().IsUnderLocation(CursorPosition);
		};
		if (IsUnderCursor(Button_Inventory)) OnClickInventoryTab();
		else if (IsUnderCursor(Button_Skills)) OnClickSkillsTab();
	}
	// Do not let an unrelated focused tab consume the same confirmation.
	return FReply::Handled();
}


void UTAInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (!SlotWidgetClass)
	{
		SlotWidgetClass = UTAInventorySlotWidget::StaticClass();
	}
	if (!ClothingPanelClass)
	{
		ClothingPanelClass = UTAClothingPanelWidget::StaticClass();
	}

	EnsureDynamicChildren();

	if (Button_Inventory)
	{
		Button_Inventory->OnClicked.RemoveDynamic(this, &UTAInventoryPanelWidget::OnClickInventoryTab);
		Button_Inventory->OnClicked.AddDynamic(this, &UTAInventoryPanelWidget::OnClickInventoryTab);
	}
	if (Button_Skills)
	{
		Button_Skills->OnClicked.RemoveDynamic(this, &UTAInventoryPanelWidget::OnClickSkillsTab);
		Button_Skills->OnClicked.AddDynamic(this, &UTAInventoryPanelWidget::OnClickSkillsTab);
	}

	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(0);
	}
	EnsureInventoryInputActions();
	PushInventoryMappingContext();
	BindInventoryInputActions();
	BuildActionPromptBar();

	if (UGameInstance* GI = GetGameInstance())
	{
		InputIconSubsystem = GI->GetSubsystem<UTAInputIconSubsystem>();
		if (InputIconSubsystem)
		{
			InputIconSubsystem->OnInputDeviceChanged.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInputDeviceChanged);
			if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
			{
				InputIconSubsystem->RefreshCurrentDeviceForUser(LocalPlayer->GetPlatformUserId());
			}
		}
	}
	RefreshInputIcons();

	RefreshLocalizedChrome();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddDynamic(this, &UTAInventoryPanelWidget::HandleLanguageChanged);
		}
	}

}

void UTAInventoryPanelWidget::NativeDestruct()
{
	PopInventoryMappingContext();
	UnbindInventoryInputActions();
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputDeviceChanged.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInputDeviceChanged);
		InputIconSubsystem = nullptr;
	}
	if (Button_Inventory)
	{
		Button_Inventory->OnClicked.RemoveDynamic(this, &UTAInventoryPanelWidget::OnClickInventoryTab);
	}
	if (Button_Skills)
	{
		Button_Skills->OnClicked.RemoveDynamic(this, &UTAInventoryPanelWidget::OnClickSkillsTab);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleLanguageChanged);
		}
	}

	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	Super::NativeDestruct();
}

void UTAInventoryPanelWidget::EnsureDynamicChildren()
{
	if (!SlotWidgetClass)
	{
		SlotWidgetClass = UTAInventorySlotWidget::StaticClass();
	}
	if (!ClothingPanelClass)
	{
		ClothingPanelClass = UTAClothingPanelWidget::StaticClass();
	}
	auto SpawnSlotInto = [&](UNamedSlot* Named, TObjectPtr<UTAInventorySlotWidget>& OutSlot)
		{
			if (!Named || OutSlot)
			{
				return;
			}
			OutSlot = CreateWidget<UTAInventorySlotWidget>(this, SlotWidgetClass);
			if (OutSlot)
			{
				Named->ClearChildren();
				Named->AddChild(OutSlot);
				OutSlot->SetEmpty();
			}
		};

	SpawnSlotInto(NamedSlot_OuterEquip, OuterEquipSlot);

	StorySlots.SetNum(4);
	SpawnSlotInto(NamedSlot_Story0, StorySlots[0]);
	SpawnSlotInto(NamedSlot_Story1, StorySlots[1]);
	SpawnSlotInto(NamedSlot_Story2, StorySlots[2]);
	SpawnSlotInto(NamedSlot_Story3, StorySlots[3]);

	if (NamedSlot_InnerClothing && !InnerClothingPanel)
	{
		InnerClothingPanel = CreateWidget<UTAClothingPanelWidget>(this, ClothingPanelClass);
		if (InnerClothingPanel)
		{
			InnerClothingPanel->SlotWidgetClass = SlotWidgetClass;
			NamedSlot_InnerClothing->ClearChildren();
			NamedSlot_InnerClothing->AddChild(InnerClothingPanel);
		}
	}

	if (NamedSlot_OuterClothing && !OuterClothingPanel)
	{
		OuterClothingPanel = CreateWidget<UTAClothingPanelWidget>(this, ClothingPanelClass);
		if (OuterClothingPanel)
		{
			OuterClothingPanel->SlotWidgetClass = SlotWidgetClass;
			NamedSlot_OuterClothing->ClearChildren();
			NamedSlot_OuterClothing->AddChild(OuterClothingPanel);
			OuterClothingPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTAInventoryPanelWidget::Init(UTAInventoryComponent* InInventory)
{
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	Inventory = InInventory;
	EnsureDynamicChildren();

	if (Inventory)
	{
		Inventory->OnInventoryUpdated.AddDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	RefreshAll();
}

void UTAInventoryPanelWidget::HandleInventoryUpdated()
{
	RefreshAll();
}

void UTAInventoryPanelWidget::OnClickInventoryTab()
{
	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(0);
	}
}

void UTAInventoryPanelWidget::OnClickSkillsTab()
{
	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(1);
	}
}

void UTAInventoryPanelWidget::EnsureInventoryInputActions()
{
	auto MakeRuntimeAction = [this](TObjectPtr<UInputAction>& Action, const TCHAR* Name)
	{
		if (!Action)
		{
			Action = NewObject<UInputAction>(this, Name);
			if (Action)
			{
				Action->ValueType = EInputActionValueType::Boolean;
			}
		}
	};

	MakeRuntimeAction(PreviousPageAction, TEXT("Runtime_InventoryPreviousPage"));
	MakeRuntimeAction(NextPageAction, TEXT("Runtime_InventoryNextPage"));
	MakeRuntimeAction(ConfirmAction, TEXT("Runtime_InventoryConfirm"));

	if (!RuntimeInventoryMappingContext)
	{
		RuntimeInventoryMappingContext = NewObject<UInputMappingContext>(this, TEXT("Runtime_InventoryMappingContext"));
	}
	if (!RuntimeInventoryMappingContext)
	{
		return;
	}

	// Runtime defaults are only added for transient actions. Designer-created IMC mappings remain authoritative.
	if (PreviousPageAction && PreviousPageAction->GetFName() == TEXT("Runtime_InventoryPreviousPage"))
	{
		RuntimeInventoryMappingContext->MapKey(PreviousPageAction, EKeys::Left);
		RuntimeInventoryMappingContext->MapKey(PreviousPageAction, EKeys::Gamepad_DPad_Left);
	}
	if (NextPageAction && NextPageAction->GetFName() == TEXT("Runtime_InventoryNextPage"))
	{
		RuntimeInventoryMappingContext->MapKey(NextPageAction, EKeys::Right);
		RuntimeInventoryMappingContext->MapKey(NextPageAction, EKeys::Gamepad_DPad_Right);
	}
	if (ConfirmAction && ConfirmAction->GetFName() == TEXT("Runtime_InventoryConfirm"))
	{
		RuntimeInventoryMappingContext->MapKey(ConfirmAction, EKeys::Enter);
		RuntimeInventoryMappingContext->MapKey(ConfirmAction, EKeys::SpaceBar);
		RuntimeInventoryMappingContext->MapKey(ConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
	}
}

void UTAInventoryPanelWidget::PushInventoryMappingContext()
{
	if (bInventoryMappingPushed)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputSubsystem)
	{
		return;
	}
	if (InventoryMappingContext)
	{
		InputSubsystem->AddMappingContext(InventoryMappingContext, InventoryMappingPriority);
	}
	if (RuntimeInventoryMappingContext)
	{
		InputSubsystem->AddMappingContext(RuntimeInventoryMappingContext, InventoryMappingPriority + 1);
	}
	bInventoryMappingPushed = InventoryMappingContext || RuntimeInventoryMappingContext;
}

void UTAInventoryPanelWidget::PopInventoryMappingContext()
{
	if (!bInventoryMappingPushed)
	{
		return;
	}
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (InventoryMappingContext)
			{
				InputSubsystem->RemoveMappingContext(InventoryMappingContext);
			}
			if (RuntimeInventoryMappingContext)
			{
				InputSubsystem->RemoveMappingContext(RuntimeInventoryMappingContext);
			}
		}
	}
	bInventoryMappingPushed = false;
}

void UTAInventoryPanelWidget::BindInventoryInputActions()
{
	APlayerController* PC = GetOwningPlayer();
	UEnhancedInputComponent* EIC = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (!EIC)
	{
		return;
	}
	if (PreviousPageAction)
	{
		InputBindingHandles.Add(EIC->BindAction(PreviousPageAction, ETriggerEvent::Started, this, &UTAInventoryPanelWidget::PreviousPage).GetHandle());
	}
	if (NextPageAction)
	{
		InputBindingHandles.Add(EIC->BindAction(NextPageAction, ETriggerEvent::Started, this, &UTAInventoryPanelWidget::NextPage).GetHandle());
	}
	if (ConfirmAction)
	{
		InputBindingHandles.Add(EIC->BindAction(ConfirmAction, ETriggerEvent::Started, this, &UTAInventoryPanelWidget::ConfirmPageAction).GetHandle());
	}
}

void UTAInventoryPanelWidget::UnbindInventoryInputActions()
{
	APlayerController* PC = GetOwningPlayer();
	if (UEnhancedInputComponent* EIC = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr)
	{
		for (const uint32 Handle : InputBindingHandles)
		{
			EIC->RemoveBindingByHandle(Handle);
		}
	}
	InputBindingHandles.Reset();
}

void UTAInventoryPanelWidget::PreviousPage(const FInputActionValue& Value)
{
	ChangePage(-1);
}

void UTAInventoryPanelWidget::NextPage(const FInputActionValue& Value)
{
	ChangePage(1);
}

void UTAInventoryPanelWidget::ChangePage(int32 Direction)
{
	if (!WidgetSwitcher || WidgetSwitcher->GetChildrenCount() < 2)
	{
		return;
	}
	const int32 PageCount = WidgetSwitcher->GetChildrenCount();
	const int32 CurrentPage = FMath::Clamp(WidgetSwitcher->GetActiveWidgetIndex(), 0, PageCount - 1);
	const int32 NextPageIndex = (CurrentPage + Direction + PageCount) % PageCount;
	WidgetSwitcher->SetActiveWidgetIndex(NextPageIndex);
}

void UTAInventoryPanelWidget::ConfirmPageAction(const FInputActionValue& Value)
{
	OnInventoryConfirmPressed();
}

void UTAInventoryPanelWidget::OnInventoryConfirmPressed_Implementation()
{
	// Page-specific selection/activation is supplied by the inventory widget Blueprint.
}

void UTAInventoryPanelWidget::BuildActionPromptBar()
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

	auto AddPrompt = [this, PromptClass](UInputAction* Action, const FString& TextId)
	{
		if (!Action)
		{
			return;
		}
		UTAActionPromptWidget* Prompt = CreateWidget<UTAActionPromptWidget>(this, PromptClass);
		if (!Prompt)
		{
			return;
		}
		Prompt->ConfigureLocalizedPrompt(Action, TextId);
		Prompt->OnPromptClicked.AddDynamic(this, &UTAInventoryPanelWidget::OnActionPromptClicked);
		HorizontalBox_Controls->AddChildToHorizontalBox(Prompt)->SetPadding(FMargin(6.0f, 0.0f));
		ActionPromptWidgets.Add(Prompt);
	};
	AddPrompt(PreviousPageAction, PreviousPagePromptTextId);
	AddPrompt(NextPageAction, NextPagePromptTextId);
	AddPrompt(ConfirmAction, ConfirmPromptTextId);
}

void UTAInventoryPanelWidget::OnActionPromptClicked(UTAActionPromptWidget* Prompt)
{
	if (!Prompt)
	{
		return;
	}
	UInputAction* Action = Prompt->GetPromptAction();
	if (Action == PreviousPageAction)
	{
		ChangePage(-1);
	}
	else if (Action == NextPageAction)
	{
		ChangePage(1);
	}
	else if (Action == ConfirmAction)
	{
		OnInventoryConfirmPressed();
	}
}

void UTAInventoryPanelWidget::RefreshInputIcons()
{
	if (!InputIconSubsystem)
	{
		return;
	}
	if (Image_PreviousPageKey)
	{
		Image_PreviousPageKey->SetBrushFromTexture(InputIconSubsystem->GetIconForAction(PreviousPageAction));
	}
	if (Image_NextPageKey)
	{
		Image_NextPageKey->SetBrushFromTexture(InputIconSubsystem->GetIconForAction(NextPageAction));
	}
}

void UTAInventoryPanelWidget::HandleInputDeviceChanged()
{
	RefreshInputIcons();
	for (UTAActionPromptWidget* Prompt : ActionPromptWidgets)
	{
		if (Prompt)
		{
			Prompt->RefreshPrompt();
		}
	}
}

void UTAInventoryPanelWidget::RefreshAll()
{
	EnsureDynamicChildren();
	if (!Inventory)
	{
		return;
	}

	if (Text_Money)
	{
		int32 Money = 0;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (ATAPlayerState* PS = PC->GetPlayerState<ATAPlayerState>())
			{
				Money = PS->GetMoney();
			}
		}

		FString Line = FString::Printf(TEXT("细胞: %d"), Money);
		if (UGameInstance* GI = GetGameInstance())
		{
			if (const UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				const FString Fmt = Loc->GetText(TEXT("UI_Cells")).ToString();
				Line = Fmt.Replace(TEXT("{0}"), *FString::FromInt(Money));
			}
		}
		Text_Money->SetText(FText::FromString(Line));
	}

	const TArray<FTAInventorySlot>& Stories = Inventory->GetStorySlots();
	for (int32 i = 0; i < StorySlots.Num(); ++i)
	{
		if (!StorySlots[i])
		{
			continue;
		}
		if (Stories.IsValidIndex(i) && !Stories[i].IsEmpty())
		{
			StorySlots[i]->SetSlotData(Stories[i], -100 - i);
		}
		else
		{
			StorySlots[i]->SetEmpty();
		}
	}

	if (OuterEquipSlot)
	{
		OuterEquipSlot->SetEmpty(); // 后续可显示外套图标
	}

	if (InnerClothingPanel)
	{
		InnerClothingPanel->BuildFromClothing(Inventory->GetInnerClothing(), 0);
	}

	if (OuterClothingPanel)
	{
		if (Inventory->HasOuterClothing())
		{
			OuterClothingPanel->SetVisibility(ESlateVisibility::Visible);
			if (NamedSlot_OuterClothing)
			{
				NamedSlot_OuterClothing->SetVisibility(ESlateVisibility::Visible);
			}

			int32 InnerSlots = 0;
			for (const FTAPocketRuntime& P : Inventory->GetInnerClothing().Pockets)
			{
				InnerSlots += P.Slots.Num();
			}
			OuterClothingPanel->BuildFromClothing(Inventory->GetOuterClothing(), InnerSlots);
		}
		else
		{
			OuterClothingPanel->ClearPanel();
			OuterClothingPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTAInventoryPanelWidget::RefreshLocalizedChrome()
{
	const UTALocalizeSubsystem* Loc = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Loc = GI->GetSubsystem<UTALocalizeSubsystem>();
	}

	auto Apply = [Loc](UTextBlock* Block, const TCHAR* TextId, const TCHAR* Fallback)
		{
			if (!Block)
			{
				return;
			}
			if (Loc)
			{
				Block->SetText(Loc->GetText(TextId));
			}
			else
			{
				Block->SetText(FText::FromString(Fallback));
			}
		};

	Apply(Text_Inventory, TEXT("UI_Inventory"), TEXT("背包"));
	Apply(Text_Skills, TEXT("UI_Skill"), TEXT("技能"));
	Apply(Text_SkillsPlaceholder, TEXT("Common_UnavailableInThisVersion"), TEXT("当前版本中不可用"));
}

void UTAInventoryPanelWidget::HandleLanguageChanged()
{
	RefreshLocalizedChrome();
	RefreshAll();
}
