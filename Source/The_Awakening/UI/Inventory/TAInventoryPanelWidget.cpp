#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TAPlayerState.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Scan/TAScanInfoWidget.h"
#include "Scan/TAScanTypes.h"
#include "The_AwakeningPlayerController.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"

FReply UTAInventoryPanelWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	// Covered owners must not receive stale focus input through Blueprint handlers.
	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) return FReply::Handled();
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
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


	SetIsFocusable(true);
	if (!InputRequestHandle)
		if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
		{
			FTAInputRequest Request;
			Request.Owner = this;
			Request.Priority = 200;
			Request.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor, ETAInputCapability::Confirm,
				ETAInputCapability::InventoryToggle, ETAInputCapability::Close, ETAInputCapability::ToggleDrag};
			Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
			Request.Presentation.bShowCursor = true;
			Request.Presentation.Focus = ETAInputFocusRequirement::Target;
			Request.Presentation.FocusTarget = this;
			InputRequestController = PC;
			InputRequestHandle = PC->AcquireInputRequest(Request);
		}
}

void UTAInventoryPanelWidget::NativeDestruct()
{
	ReleaseInput();
	CancelGamepadDragMode();
	if (ItemInfoWidget)
	{
		ItemInfoWidget->RemoveFromParent();
		ItemInfoWidget = nullptr;
	}
	HoveredItemSlot.Reset();
	PopInventoryMappingContext();
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

void UTAInventoryPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bRefreshInputPromptsNextTick)
	{
		bRefreshInputPromptsNextTick = false;
		RefreshInputIcons();
	}
	const bool bItemDragActive = bGamepadDragModeActive || UWidgetBlueprintLibrary::IsDragDropping();
	if (bItemDragActive)
	{
		HideItemInfo(HoveredItemSlot.Get());
	}
	else if (bWasItemDragActive)
	{
		if (UTAInventorySlotWidget* HoveredSlot = CurrentHoveredInventorySlot.Get(); HoveredSlot && HoveredSlot->IsHovered() && !HoveredSlot->IsEmpty())
		{
			ShowItemInfo(HoveredSlot, HoveredSlot->GetItemDef());
		}
	}
	bWasItemDragActive = bItemDragActive;
	UpdateItemInfoWidgetPosition();
	UpdateGamepadDragVisualPosition();
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
				BindSlotHoverEvents(OutSlot);
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
			InnerClothingPanel->SetInventoryComponent(Inventory);
			InnerClothingPanel->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotHovered);
			InnerClothingPanel->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotUnhovered);
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
			OuterClothingPanel->SetInventoryComponent(Inventory);
			OuterClothingPanel->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotHovered);
			OuterClothingPanel->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotUnhovered);
			NamedSlot_OuterClothing->ClearChildren();
			NamedSlot_OuterClothing->AddChild(OuterClothingPanel);
			OuterClothingPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTAInventoryPanelWidget::Init(UTAInventoryComponent* InInventory, UInputAction* InCloseAction)
{
	CloseAction = InCloseAction;
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	Inventory = InInventory;
	EnsureDynamicChildren();
	if (OuterEquipSlot)
	{
		OuterEquipSlot->SetInventoryComponent(Inventory);
	}
	for (UTAInventorySlotWidget* StorySlot : StorySlots)
	{
		if (StorySlot)
		{
			StorySlot->SetInventoryComponent(Inventory);
		}
	}
	if (InnerClothingPanel)
	{
		InnerClothingPanel->SetInventoryComponent(Inventory);
	}
	if (OuterClothingPanel)
	{
		OuterClothingPanel->SetInventoryComponent(Inventory);
	}

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
	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) return;
	CancelGamepadDragMode();
	HideItemInfo(HoveredItemSlot.Get());
	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(0);
	}
}

void UTAInventoryPanelWidget::OnClickSkillsTab()
{
	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) return;
	CancelGamepadDragMode();
	HideItemInfo(HoveredItemSlot.Get());
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
	MakeRuntimeAction(GamepadDragModeAction, TEXT("Runtime_InventoryGamepadDragMode"));

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
	if (GamepadDragModeAction && GamepadDragModeAction->GetFName() == TEXT("Runtime_InventoryGamepadDragMode"))
	{
		RuntimeInventoryMappingContext->MapKey(GamepadDragModeAction, EKeys::Gamepad_FaceButton_Right);
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

void UTAInventoryPanelWidget::ChangePage(int32 Direction)
{
	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) return;
	if (!WidgetSwitcher || WidgetSwitcher->GetChildrenCount() < 2)
	{
		return;
	}
	const int32 PageCount = WidgetSwitcher->GetChildrenCount();
	const int32 CurrentPage = FMath::Clamp(WidgetSwitcher->GetActiveWidgetIndex(), 0, PageCount - 1);
	const int32 NextPageIndex = (CurrentPage + Direction + PageCount) % PageCount;
	if (NextPageIndex != CurrentPage)
	{
		CancelGamepadDragMode();
	}
	WidgetSwitcher->SetActiveWidgetIndex(NextPageIndex);
}

void UTAInventoryPanelWidget::ToggleGamepadDragMode(const FInputActionValue& Value)
{
	if (!AllowsPlayerInput(ETAInputCapability::ToggleDrag)) return;
	if (InputIconSubsystem && InputIconSubsystem->GetCurrentDeviceType() == EInputDeviceType::KeyboardMouse)
	{
		return;
	}

	if (bGamepadDragModeActive)
	{
		CommitGamepadDragMode();
		return;
	}
	BeginGamepadDragMode(CurrentHoveredInventorySlot.Get());
}

void UTAInventoryPanelWidget::HandleConfirmPressed()
{
	if (!AllowsPlayerInput(ETAInputCapability::Confirm)) return;
	// A synthetic click can hit this very prompt; reuse pointer synthesis's reentrancy boundary.
	if (const auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()); PC && PC->IsSimulatingSyntheticLeftMouseClick()) return;
	if (bGamepadDragModeActive)
	{
		CommitGamepadDragMode();
	}
	else
	{
		SimulateLeftMouseClick();
	}
}

void UTAInventoryPanelWidget::BindSlotHoverEvents(UTAInventorySlotWidget* InventorySlot)
{
	if (!InventorySlot)
	{
		return;
	}
	InventorySlot->SetInventoryComponent(Inventory);
	InventorySlot->SetInputOwner(this);
	InventorySlot->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotHovered);
	InventorySlot->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotUnhovered);
}

void UTAInventoryPanelWidget::HandleInventorySlotHovered(UTAInventorySlotWidget* HoveredSlot, UTAItemDefinition* ItemDef)
{
	if (!AllowsPlayerInput(ETAInputCapability::Cursor)) return;
	CurrentHoveredInventorySlot = HoveredSlot;
	ShowItemInfo(HoveredSlot, ItemDef);
}

void UTAInventoryPanelWidget::HandleInventorySlotUnhovered(UTAInventorySlotWidget* HoveredSlot)
{
	if (CurrentHoveredInventorySlot.Get() == HoveredSlot)
	{
		CurrentHoveredInventorySlot.Reset();
	}
	HideItemInfo(HoveredSlot);
}

void UTAInventoryPanelWidget::BeginGamepadDragMode(UTAInventorySlotWidget* SourceSlot)
{
	if (!SourceSlot || SourceSlot->IsEmpty() || !SourceSlot->GetItemDef() || !Inventory)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (!GamepadDragVisualWidget)
	{
		GamepadDragVisualWidget = CreateWidget<UTAInventorySlotWidget>(PC, SlotWidgetClass);
	}
	if (!GamepadDragVisualWidget)
	{
		return;
	}

	GamepadDragVisualWidget->SetSlotData(SourceSlot->GetSlotData(), SourceSlot->GetFlatIndex());
	GamepadDragVisualWidget->SetInventoryComponent(Inventory);
	GamepadDragVisualWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	GamepadDragVisualWidget->AddToViewport(1000001);
	GamepadDragVisualWidget->SetAlignmentInViewport(FVector2D::ZeroVector);

	GamepadDragSourceSlot = SourceSlot;
	bGamepadDragModeActive = true;
	SourceSlot->SetDraggingVisual(true);
	HideItemInfo(HoveredItemSlot.Get());
	UpdateGamepadDragVisualPosition();
}

void UTAInventoryPanelWidget::CancelGamepadDragMode()
{
	if (UTAInventorySlotWidget* SourceSlot = GamepadDragSourceSlot.Get())
	{
		SourceSlot->SetDraggingVisual(false);
	}
	GamepadDragSourceSlot.Reset();
	bGamepadDragModeActive = false;
	if (GamepadDragVisualWidget)
	{
		GamepadDragVisualWidget->RemoveFromParent();
		GamepadDragVisualWidget = nullptr;
	}
}

void UTAInventoryPanelWidget::CommitGamepadDragMode()
{
	UTAInventorySlotWidget* SourceSlot = GamepadDragSourceSlot.Get();
	UTAInventorySlotWidget* TargetSlot = CurrentHoveredInventorySlot.Get();
	if (!SourceSlot || SourceSlot->IsEmpty() || !TargetSlot)
	{
		CancelGamepadDragMode();
		return;
	}
	if (SourceSlot == TargetSlot)
	{
		CancelGamepadDragMode();
		return;
	}

	if (Inventory)
	{
		Inventory->MoveItemBetweenSlots(SourceSlot->GetFlatIndex(), TargetSlot->GetFlatIndex());
	}
	CancelGamepadDragMode();
}

void UTAInventoryPanelWidget::UpdateGamepadDragVisualPosition()
{
	if (!bGamepadDragModeActive || !GamepadDragVisualWidget || !GetOwningPlayer())
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PC->GetViewportSize(ViewportWidth, ViewportHeight);
	float CursorX = 0.0f;
	float CursorY = 0.0f;
	const auto* CursorController = Cast<AThe_AwakeningPlayerController>(PC);
	const bool bHasCursorPosition = CursorController
		? CursorController->GetPlayerCursorPosition(CursorX, CursorY)
		: PC->GetMousePosition(CursorX, CursorY);
	if (!bHasCursorPosition)
	{
		return;
	}

	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PC), KINDA_SMALL_NUMBER);
	GamepadDragVisualWidget->SetPositionInViewport(
		FVector2D(CursorX / ViewportScale + 14.0f, CursorY / ViewportScale + 14.0f), false);
}

void UTAInventoryPanelWidget::ShowItemInfo(UTAInventorySlotWidget* InventorySlot, UTAItemDefinition* ItemDef)
{
	if (bGamepadDragModeActive || UWidgetBlueprintLibrary::IsDragDropping())
	{
		HideItemInfo(HoveredItemSlot.Get());
		return;
	}
	if (!InventorySlot || !ItemDef)
	{
		HideItemInfo(InventorySlot);
		return;
	}

	HoveredItemSlot = InventorySlot;
	if (!ItemInfoWidgetClass)
	{
		ItemInfoWidgetClass = LoadClass<UTAScanInfoWidget>(nullptr, TEXT("/Game/UI/Scan/WBP_ScanInfo.WBP_ScanInfo_C"));
	}
	if (!ItemInfoWidget && ItemInfoWidgetClass)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			ItemInfoWidget = CreateWidget<UTAScanInfoWidget>(PC, ItemInfoWidgetClass);
			if (ItemInfoWidget)
			{
				ItemInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
				// AddToViewport adds 10 to the supplied ZOrder internally, so keep this high without overflowing.
				ItemInfoWidget->AddToViewport(1000000);
				ItemInfoWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
			}
		}
	}
	if (!ItemInfoWidget)
	{
		return;
	}

	FTAScanTargetInfo Info;
	Info.Name = ItemDef->DisplayName;
	Info.Description = ItemDef->Description;
	Info.TargetType = ETAScanTargetType::Item;
	ItemInfoWidget->SetTargetInfo(Info);
	// The card is informational only and must not intercept the cursor leaving the slot.
	ItemInfoWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	UpdateItemInfoWidgetPosition();
}

void UTAInventoryPanelWidget::HideItemInfo(UTAInventorySlotWidget* InventorySlot)
{
	if (HoveredItemSlot.IsValid() && InventorySlot && HoveredItemSlot.Get() != InventorySlot)
	{
		return;
	}
	HoveredItemSlot.Reset();
	if (ItemInfoWidget)
	{
		ItemInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTAInventoryPanelWidget::UpdateItemInfoWidgetPosition()
{
	APlayerController* PC = GetOwningPlayer();
	if (!ItemInfoWidget || !ItemInfoWidget->IsVisible() || !HoveredItemSlot.IsValid() || !PC)
	{
		return;
	}

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	const auto* CursorController = Cast<AThe_AwakeningPlayerController>(PC);
	if (!(CursorController ? CursorController->GetPlayerCursorPosition(CursorX, CursorY) : PC->GetMousePosition(CursorX, CursorY)))
	{
		CursorX = ViewportX * 0.5f;
		CursorY = ViewportY * 0.5f;
	}

	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PC), KINDA_SMALL_NUMBER);
	ItemInfoWidget->ForceLayoutPrepass();
	const FVector2D DesiredSize = ItemInfoWidget->GetDesiredSize();
	const FVector2D LogicalViewport(ViewportX / ViewportScale, ViewportY / ViewportScale);
	FVector2D Position(CursorX / ViewportScale + ItemInfoCursorOffset.X, CursorY / ViewportScale + ItemInfoCursorOffset.Y);
	Position.X = FMath::Clamp(Position.X, 0.0f, FMath::Max(0.0f, LogicalViewport.X - DesiredSize.X));
	Position.Y = FMath::Clamp(Position.Y, 0.0f, FMath::Max(0.0f, LogicalViewport.Y - DesiredSize.Y));
	ItemInfoWidget->SetPositionInViewport(Position, false);
}

void UTAInventoryPanelWidget::SimulateLeftMouseClick()
{
	if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		PC->SimulateSyntheticLeftMouseClick();
	}
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
	AddPrompt(GamepadDragModeAction, GamepadDragModePromptTextId);
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
		HandleConfirmPressed();
	}
	else if (Action == GamepadDragModeAction)
	{
		FInputActionValue Value(true);
		ToggleGamepadDragMode(Value);
	}
}

void UTAInventoryPanelWidget::RefreshInputIcons()
{
	if (InputIconSubsystem)
	{
		if (Image_PreviousPageKey)
		{
			FTAPromptWidgetUtils::ApplyKeyIcon(
				Image_PreviousPageKey,
				InputIconSubsystem->GetIconForAction(PreviousPageAction),
				PageShortcutIconHeight,
				SizeBox_PreviousPageKey);
		}
		if (Image_NextPageKey)
		{
			FTAPromptWidgetUtils::ApplyKeyIcon(
				Image_NextPageKey,
				InputIconSubsystem->GetIconForAction(NextPageAction),
				PageShortcutIconHeight,
				SizeBox_NextPageKey);
		}
	}
	for (UTAActionPromptWidget* Prompt : ActionPromptWidgets)
	{
		if (Prompt)
		{
			Prompt->RefreshPrompt();
			if (Prompt->GetPromptAction() == GamepadDragModeAction)
			{
				const bool bIsGamepad = InputIconSubsystem
					&& InputIconSubsystem->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
				Prompt->SetVisibility(bIsGamepad ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		}
	}
}

void UTAInventoryPanelWidget::RefreshInputPrompts()
{
	RefreshInputIcons();
	// Enhanced Input may not have rebuilt mappings until the next frame after the IMC is pushed.
	bRefreshInputPromptsNextTick = true;
}

void UTAInventoryPanelWidget::HandleInputDeviceChanged()
{
	RefreshInputPrompts();
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

void UTAInventoryPanelWidget::ReleaseInput()
{
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
		if (auto* PC = Issuer.Get()) PC->ReleaseInputRequest(Handle);
}

void UTAInventoryPanelWidget::RemoveFromParent()
{
	ReleaseInput();
	Super::RemoveFromParent();
}

bool UTAInventoryPanelWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

TOptional<ETAInputCapability> UTAInventoryPanelWidget::ResolvePlayerInput(FKey Key) const
{
	const auto* PC = InputRequestController.Get();
	if (!PC) return {};
	if (PC->IsKeyMappedToAction(Key, CloseAction)) return ETAInputCapability::InventoryToggle;
	if (PC->IsKeyMappedToAction(Key, PreviousPageAction) || PC->IsKeyMappedToAction(Key, NextPageAction)) return ETAInputCapability::Navigate;
	if (PC->IsKeyMappedToAction(Key, ConfirmAction)) return ETAInputCapability::Confirm;
	if (PC->IsKeyMappedToAction(Key, GamepadDragModeAction)) return ETAInputCapability::ToggleDrag;
	return {};
}

void UTAInventoryPanelWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	if (!AllowsPlayerInput(Capability)) return;
	const auto* PC = InputRequestController.Get();
	if (Capability == ETAInputCapability::InventoryToggle)
	{
		if (InputRequestController->ConsumeInventoryTogglePress(CloseAction)) RemoveFromParent();
	}
	else if (Capability == ETAInputCapability::Navigate) ChangePage(PC->IsKeyMappedToAction(Key, PreviousPageAction) ? -1 : 1);
	else if (Capability == ETAInputCapability::Confirm) HandleConfirmPressed();
	else if (Capability == ETAInputCapability::ToggleDrag) ToggleGamepadDragMode(FInputActionValue(true));
}
