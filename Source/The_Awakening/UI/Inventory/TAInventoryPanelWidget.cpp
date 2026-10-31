#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAInventoryTabBar.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TAPlayerState.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Health/TAHealthComponent.h"
#include "Engine/Texture2D.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "GameFramework/PlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Engine/Engine.h"
#include "Core/TAInputIconSubsystem.h"
#include "Scan/TAScanInfoWidget.h"
#include "Scan/TAScanTypes.h"
#include "The_AwakeningPlayerController.h"
#include "The_AwakeningCharacter.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Core/TATimeSubsystem.h"
#include "UI/Inventory/TAItemActionMenuWidget.h"
#include "UI/Inventory/TAQuantityDialogWidget.h"
#include "UI/Inventory/TAItemInspectWidget.h"
#include "UI/Document/TADiaryWidget.h"
#include "Document/TADocumentSettings.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

int32 UTAInventoryPanelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, CullingRect, Out, Layer, Style, bParentEnabled);
	const FVector2D Size = Geometry.GetLocalSize();
	const float Scale = FMath::Max(.5f, float(Size.Y / 1080.f));
	const FLinearColor Color = FLinearColor(FColor(0x74, 0xB9, 0xB0)) * Style.GetColorAndOpacityTint();
	auto Point = [&](float X, float Y) { return FVector2D(Size.X * X, Size.Y * Y); };
	auto Line = [&](FVector2D From, FVector2D To, float Width)
	{
		const TArray<FVector2D> Points = { From, To };
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * .06f), true, (Width + 5.f) * Scale);
		FSlateDrawElement::MakeLines(Out, Layer + 2, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * .12f), true, (Width + 2.f) * Scale);
		FSlateDrawElement::MakeLines(Out, Layer + 3, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color, true, Width * Scale);
	};
	auto Cap = [&](FVector2D At, FVector2D Extent)
	{
		FSlateDrawElement::MakeBox(Out, Layer + 2, Geometry.ToPaintGeometry(Extent + FVector2D(4.f * Scale),
			FSlateLayoutTransform(At - FVector2D(2.f * Scale))), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
			ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * .08f));
		FSlateDrawElement::MakeBox(Out, Layer + 3, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(At)),
			FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color);
	};
	// Normalized measurements from the reference: the sides intentionally start/end at different heights.
	Line(Point(.041f, .140f), Point(.041f, .915f), 1.f);
	Cap(Point(.041f, .133f), Point(.014f, .014f));
	Cap(Point(.041f, .912f), Point(.003f, .005f));
	Line(Point(.952f, .105f), Point(.952f, .950f), 1.f);
	Cap(Point(.935f, .098f), Point(.017f, .014f));
	Cap(Point(.944f, .947f), Point(.008f, .007f));
	Line(Point(.209f, .952f), Point(.575f, .952f), 1.5f);
	return Layer + 3;
}

FReply UTAInventoryPanelWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	// Covered owners must not receive stale focus input through Blueprint handlers.
	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) return FReply::Handled();
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UTAInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ItemActionsAction)
	{
		ItemActionsAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/UI/IA_ItemActions.IA_ItemActions"));
	}
	if (Text_Message)
	{
		Text_Message->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	if (TabBar_Inventory)
	{
		TabBar_Inventory->OnTabRequested.AddUniqueDynamic(this, &UTAInventoryPanelWidget::SelectInventoryPage);
		TabBar_Inventory->SetPage(0, false);
	}
	EnsureInventoryInputActions();
	BuildActionPromptBar();

	if (UGameInstance* GI = GetGameInstance())
	{
		InputIconSubsystem = GI->GetSubsystem<UTAInputIconSubsystem>();
		if (InputIconSubsystem)
		{
			InputIconSubsystem->OnInputPromptsChanged.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInputPromptsChanged);
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
	// 时停：只冻结挂了冻结组件的角色和敌人，UI 动画照常；扫描中打开时由背包接管，冻结不会出现空档
	if (!bFreezeRequested)
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
		{
			Time->RequestFreeze(this, 1.f);
			bFreezeRequested = true;
		}
	// Child Slate trees may rebuild after Init; populate the final constructed tree.
	RefreshAll();
}

void UTAInventoryPanelWidget::NativeDestruct()
{
	if (TabBar_Inventory)
	{
		TabBar_Inventory->OnTabRequested.RemoveDynamic(this, &UTAInventoryPanelWidget::SelectInventoryPage);
	}
	ReleaseInput();
	CancelGamepadDragMode();
	if (ItemInfoWidget)
	{
		ItemInfoWidget->RemoveFromParent();
		ItemInfoWidget = nullptr;
	}
	HoveredItemSlot.Reset();
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputPromptsChanged.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInputPromptsChanged);
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
	RefreshHeaderStats();
	if (NamedSlot_OuterEquip && NamedSlot_Story0 && InnerClothingPanel && WidgetSwitcher && WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		UWidget* InnerFrame = InnerClothingPanel->GetWidgetFromName(TEXT("InnerPocketFrame"));
		auto* Placement = Cast<UCanvasPanelSlot>(NamedSlot_OuterEquip->Slot);
		UWidget* Parent = NamedSlot_OuterEquip->GetParent();
		const auto Story = NamedSlot_Story0->GetCachedGeometry();
		if (InnerFrame && Placement && Parent && MyGeometry.GetLocalSize().X > 0.f && Story.GetLocalSize().X > 0.f && InnerFrame->GetCachedGeometry().GetLocalSize().Y > 0.f)
		{
			const FVector2D StoryTopLeft = MyGeometry.AbsoluteToLocal(Story.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D StorySize = MyGeometry.AbsoluteToLocal(Story.LocalToAbsolute(Story.GetLocalSize())) - StoryTopLeft;
			const float Gap = FMath::Max(0.f, float(StoryTopLeft.X - MyGeometry.GetLocalSize().X * .041f));
			const auto Inner = InnerFrame->GetCachedGeometry();
			const float RowCenter = MyGeometry.AbsoluteToLocal(Inner.LocalToAbsolute(Inner.GetLocalSize() * .5f)).Y;
			const FVector2D At(MyGeometry.GetLocalSize().X * .952f - Gap - StorySize.X, RowCenter - StorySize.Y * .5f);
			const auto Canvas = Parent->GetCachedGeometry();
			const FVector2D LocalAt = Canvas.AbsoluteToLocal(MyGeometry.LocalToAbsolute(At));
			const FVector2D LocalSize = Canvas.AbsoluteToLocal(MyGeometry.LocalToAbsolute(At + StorySize)) - LocalAt;
			if (!Placement->GetPosition().Equals(LocalAt, .1f)) Placement->SetPosition(LocalAt);
			if (!Placement->GetSize().Equals(LocalSize, .1f)) Placement->SetSize(LocalSize);
		}
	}
	if (MessageTimeLeft > 0.f)
	{
		MessageTimeLeft -= InDeltaTime;
		if (MessageTimeLeft <= 0.f && Text_Message)
		{
			Text_Message->SetVisibility(ESlateVisibility::Collapsed);
		}
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
	if (OuterEquipSlot)
	{
		// 外套只能在衣架换装界面穿脱，背包里的外套格只显示
		OuterEquipSlot->SetDragEnabled(false);
		if (OuterEquipBackgroundTexture)
		{
			if (auto* Frame = Cast<UBorder>(OuterEquipSlot->GetWidgetFromName(TEXT("Border"))))
			{
				FSlateBrush Brush = Frame->Background;
				Brush.SetResourceObject(OuterEquipBackgroundTexture);
				Frame->SetBrush(Brush); // Preserve the existing blue tint and nine-slice margins.
			}
		}
	}

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
			InnerClothingPanel->SetWornInventoryLayout();
			InnerClothingPanel->SlotWidgetClass = SlotWidgetClass;
			InnerClothingPanel->SetInventoryComponent(Inventory);
			InnerClothingPanel->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotHovered);
			InnerClothingPanel->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAInventoryPanelWidget::HandleInventorySlotUnhovered);
			NamedSlot_InnerClothing->ClearChildren();
			NamedSlot_InnerClothing->AddChild(InnerClothingPanel);
		}
	}

	// The inventory now has one combined clothing/pocket presentation.
	if (NamedSlot_OuterClothing) NamedSlot_OuterClothing->SetVisibility(ESlateVisibility::Collapsed);
	if (NamedSlot_InnerClothing)
	{
		for (UNamedSlot* Named : { NamedSlot_InnerClothing.Get(), NamedSlot_OuterEquip.Get() })
		{
			if (!Named || Cast<UCanvasPanelSlot>(Named->Slot)) continue;
			// Keep the original page and story column; clothing and equipment use its canvas.
			for (UWidget* Parent = Named->GetParent(); Parent; Parent = Parent->GetParent())
			{
				if (auto* PageCanvas = Cast<UCanvasPanel>(Parent))
				{
					Named->RemoveFromParent();
					PageCanvas->AddChildToCanvas(Named)->SetZOrder(1);
					break;
				}
			}
		}
		if (auto* Layout = Cast<UCanvasPanelSlot>(NamedSlot_InnerClothing->Slot))
		{
			Layout->SetAnchors(FAnchors(.19f, .16f, .912f, .91f));
			Layout->SetAlignment(FVector2D::ZeroVector);
			Layout->SetOffsets(FMargin(0.f));
			Layout->SetAutoSize(false);
		}
		if (NamedSlot_OuterEquip)
			if (auto* Layout = Cast<UCanvasPanelSlot>(NamedSlot_OuterEquip->Slot))
			{
				Layout->SetAnchors(FAnchors(0.f));
				Layout->SetAlignment(FVector2D::ZeroVector);
				Layout->SetAutoSize(false);
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
	SelectInventoryPage(0);
}

void UTAInventoryPanelWidget::OnClickSkillsTab()
{
	SelectInventoryPage(1);
}

void UTAInventoryPanelWidget::SelectInventoryPage(int32 Page)
{
	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) return;
	if (!WidgetSwitcher || Page < 0 || Page >= WidgetSwitcher->GetChildrenCount()) return;
	CancelGamepadDragMode();
	HideItemInfo(HoveredItemSlot.Get());
	if (Page == WidgetSwitcher->GetActiveWidgetIndex()) return;
	WidgetSwitcher->SetActiveWidgetIndex(Page);
	if (TabBar_Inventory) TabBar_Inventory->SetPage(Page);
}

void UTAInventoryPanelWidget::EnsureInventoryInputActions()
{
	if (!PreviousPageAction || !NextPageAction || !ConfirmAction || !GamepadDragModeAction)
	{
		UE_LOG(LogTemp, Error, TEXT("Inventory UI actions must be assigned to shared UI Input Actions in the inventory widget defaults."));
	}
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
	const int32 NextPageIndex = FMath::Clamp(CurrentPage + Direction, 0, PageCount - 1);
	if (NextPageIndex == CurrentPage) return;
	SelectInventoryPage(NextPageIndex);
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
	if (!SourceSlot || SourceSlot->IsEmpty() || SourceSlot->IsStorySlot() || !SourceSlot->GetItemDef() || !Inventory)
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
	if (!HorizontalBox_Controls) HorizontalBox_Controls = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("HorizontalBox_Controls")));
	if (HorizontalBox_Controls)
		if (auto* Root = Cast<UCanvasPanel>(GetRootWidget()))
		{
			if (HorizontalBox_Controls->GetParent() != Root)
			{
				HorizontalBox_Controls->RemoveFromParent();
				Root->AddChildToCanvas(HorizontalBox_Controls);
			}
			if (auto* Placement = Cast<UCanvasPanelSlot>(HorizontalBox_Controls->Slot))
			{
				// Inside the 95.2% rails, with a 1.2% inset on both axes.
				Placement->SetAnchors(FAnchors(.94f, .94f));
				Placement->SetAlignment(FVector2D(1.f, 1.f));
				Placement->SetOffsets(FMargin(0.f));
				Placement->SetAutoSize(true);
				Placement->SetZOrder(10);
			}
		}
	if (!HorizontalBox_Controls || ActionPromptWidgets.Num() > 0)
	{
		return;
	}
	auto AddPrompt = [this](UInputAction* Action, const FString& TextId)
	{
		if (!Action)
		{
			return;
		}
		UTAActionPromptWidget* Prompt = FTAPromptWidgetUtils::AddActionPrompt(
			this, HorizontalBox_Controls, ActionPromptWidgetClass, Action, TextId);
		if (!Prompt)
		{
			return;
		}
		Prompt->OnPromptClicked.AddDynamic(this, &UTAInventoryPanelWidget::OnActionPromptClicked);
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
		if (TabBar_Inventory)
		{
			TabBar_Inventory->SetInputIcons(
				InputIconSubsystem->GetIconForActionForPlayer(PreviousPageAction, GetOwningLocalPlayer()),
				InputIconSubsystem->GetIconForActionForPlayer(NextPageAction, GetOwningLocalPlayer()));
		}
		if (Image_PreviousPageKey)
		{
			FTAPromptWidgetUtils::ApplyKeyIcon(
				Image_PreviousPageKey,
				InputIconSubsystem->GetIconForActionForPlayer(PreviousPageAction, GetOwningLocalPlayer()),
				PageShortcutIconHeight,
				SizeBox_PreviousPageKey);
		}
		if (Image_NextPageKey)
		{
			FTAPromptWidgetUtils::ApplyKeyIcon(
				Image_NextPageKey,
				InputIconSubsystem->GetIconForActionForPlayer(NextPageAction, GetOwningLocalPlayer()),
				PageShortcutIconHeight,
				SizeBox_NextPageKey);
		}
	}
	for (UTAActionPromptWidget* Prompt : ActionPromptWidgets)
	{
		if (Prompt)
		{
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
}

void UTAInventoryPanelWidget::HandleInputPromptsChanged()
{
	RefreshInputPrompts();
}

void UTAInventoryPanelWidget::RefreshAll()
{
	RefreshHeaderStats();
	EnsureDynamicChildren();
	if (!Inventory)
	{
		return;
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
			StorySlots[i]->SetSlotData(Stories[i], UTAInventoryComponent::ToStorySlotUIIndex(i));
		}
		else
		{
			StorySlots[i]->SetEmpty();
		}
	}

	const UTAClothingDefinition* WornOuter = Inventory->HasOuterClothing() ? Inventory->GetOuterClothing().Definition.Get() : nullptr;
	if (OuterEquipSlot)
	{
		OuterEquipSlot->SetEmpty();
		OuterEquipSlot->SetClothingDisplay(const_cast<UTAClothingDefinition*>(WornOuter));
	}

	if (InnerClothingPanel)
	{
		InnerClothingPanel->BuildWornClothing(Inventory->GetInnerClothing(), Inventory->GetOuterClothing());
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
	if (TabBar_Inventory && Text_Inventory && Text_Skills)
	{
		TabBar_Inventory->SetLabels(Text_Inventory->GetText(), Text_Skills->GetText(), Text_Inventory->GetFont());
	}
	RefreshHeaderStats(true);
}

void UTAInventoryPanelWidget::RefreshHeaderStats(bool bForce)
{
	if (!TabBar_Inventory) return;
	APlayerController* PC = GetOwningPlayer();
	const ATAPlayerState* PS = PC ? PC->GetPlayerState<ATAPlayerState>() : nullptr;
	const int32 Cells = PS ? PS->GetMoney() : 0;
	int32 Health = INDEX_NONE;
	const UTAHealthComponent* HealthComponent = UTAHealthComponent::FindHealthComponent(GetOwningPlayerPawn());
	if (HealthComponent && HealthComponent->GetAbilitySystemComponent())
		Health = FMath::RoundToInt(FMath::Max(0.f, HealthComponent->GetHealth()));
	else if (PS && PS->GetAttributeSet())
		Health = FMath::RoundToInt(FMath::Max(0.f, PS->GetAttributeSet()->GetHealth()));
	if (!bForce && bHeaderStatsInitialized && Cells == CachedHeaderCells && Health == CachedHeaderHealth) return;
	CachedHeaderCells = Cells; CachedHeaderHealth = Health; bHeaderStatsInitialized = true;
	FString CellsPattern = TEXT("细胞: {0}");
	FString HealthPattern = TEXT("HP: {0}");
	if (UGameInstance* GI = GetGameInstance())
		if (const auto* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			CellsPattern = Loc->GetText(TEXT("UI_Cells")).ToString();
			HealthPattern = Loc->GetText(TEXT("UI_Inventory_HP")).ToString();
		}
	TabBar_Inventory->SetStatusLabels(
		FText::FromString(CellsPattern.Replace(TEXT("{0}"), *FString::FromInt(Cells))),
		FText::FromString(HealthPattern.Replace(TEXT("{0}"), Health == INDEX_NONE ? TEXT("—") : *FString::FromInt(Health))));
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
	if (bFreezeRequested)
	{
		bFreezeRequested = false;
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this)) Time->ReleaseFreeze(this);
	}
}

void UTAInventoryPanelWidget::RemoveFromParent()
{
	ReleaseInput();
	if (AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(GetOwningPlayerPawn()))
	{
		Character->NotifyInventoryPanelClosed(this);
	}
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
	// 物品交互先于确认判断：两者共用 Confirm 权限
	if (ItemActionsAction && PC->IsKeyMappedToAction(Key, ItemActionsAction)) return ETAInputCapability::Confirm;
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
	else if (Capability == ETAInputCapability::Confirm && ItemActionsAction && PC->IsKeyMappedToAction(Key, ItemActionsAction))
		OpenItemActionMenu(CurrentHoveredInventorySlot.Get());
	else if (Capability == ETAInputCapability::Confirm) HandleConfirmPressed();
	else if (Capability == ETAInputCapability::ToggleDrag) ToggleGamepadDragMode(FInputActionValue(true));
}

// ==================== 物品交互菜单 / 日记本 ====================

void UTAInventoryPanelWidget::OpenItemActionMenu(UTAInventorySlotWidget* InventorySlot)
{
	if (!InventorySlot || InventorySlot->IsEmpty() || bGamepadDragModeActive || !Inventory)
	{
		return;
	}
	TArray<FTAItemActionOption> Options = UTAItemActionMenuWidget::GetOptionsForSlot(InventorySlot->GetSlotData(), Inventory->HasEmptySlot());
	if (Options.IsEmpty())
	{
		return;
	}
	HideItemInfo(InventorySlot);
	UTAItemActionMenuWidget::Open(GetOwningPlayer(), nullptr, MoveTemp(Options),
		[WeakThis = TWeakObjectPtr<UTAInventoryPanelWidget>(this), WeakSlot = TWeakObjectPtr<UTAInventorySlotWidget>(InventorySlot)](FName ActionId)
		{
			if (UTAInventoryPanelWidget* Self = WeakThis.Get())
			{
				Self->HandleItemAction(ActionId, WeakSlot);
			}
		});
}

void UTAInventoryPanelWidget::HandleItemAction(FName ActionId, TWeakObjectPtr<UTAInventorySlotWidget> SourceSlot)
{
	if (ActionId == UTAItemActionMenuWidget::ActionViewContent)
	{
		// 书从剧情格里日记本的位置放大，关闭时缩回去
		TOptional<FVector2D> Source, SourceSize;
		if (const UTAInventorySlotWidget* SlotWidget = SourceSlot.Get())
		{
			const FGeometry& Geometry = SlotWidget->GetIconGeometry();
			if (Geometry.GetLocalSize().GetMin() > 1.f)
			{
				FVector2D PixelPosition, Center, Min, Max;
				USlateBlueprintLibrary::AbsoluteToViewport(this, Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f), PixelPosition, Center);
				USlateBlueprintLibrary::AbsoluteToViewport(this, Geometry.LocalToAbsolute(FVector2D::ZeroVector), PixelPosition, Min);
				USlateBlueprintLibrary::AbsoluteToViewport(this, Geometry.LocalToAbsolute(Geometry.GetLocalSize()), PixelPosition, Max);
				Source = Center;
				SourceSize = Max - Min;
			}
		}
		UTADiaryWidget::Open(GetOwningPlayer(), nullptr, Source, INDEX_NONE, false, false, SourceSize,
			SourceSlot.IsValid() ? SourceSlot->GetIconWidget() : nullptr);
	}
	else if (ActionId == UTAItemActionMenuWidget::ActionInspect)
	{
		if (const UTAInventorySlotWidget* SlotWidget = SourceSlot.Get(); SlotWidget && !SlotWidget->IsEmpty())
		{
			UTAItemInspectWidget::Open(GetOwningPlayer(), nullptr, SlotWidget->GetItemDef());
		}
	}
	else if (ActionId == UTAItemActionMenuWidget::ActionPartialDrop || ActionId == UTAItemActionMenuWidget::ActionSplit)
	{
		const UTAInventorySlotWidget* SlotWidget = SourceSlot.Get();
		if (!SlotWidget || !Inventory || SlotWidget->IsEmpty())
		{
			return;
		}
		// 记下格子下标而不是控件：背包打开时物品不会被别处改动，下标在窗口关闭前保持有效
		const int32 FlatIndex = SlotWidget->GetFlatIndex();
		const bool bSplit = ActionId == UTAItemActionMenuWidget::ActionSplit;
		UTAQuantityDialogWidget::Open(GetOwningPlayer(), nullptr, bSplit ? TEXT("UI_Item_Split") : TEXT("UI_Item_PartialDrop"),
			1, SlotWidget->GetSlotData().Count,
			[WeakThis = TWeakObjectPtr<UTAInventoryPanelWidget>(this), FlatIndex, bSplit](int32 Quantity)
			{
				UTAInventoryPanelWidget* Self = WeakThis.Get();
				if (!Self || !Self->Inventory)
				{
					return;
				}
				if (bSplit)
				{
					Self->Inventory->SplitStack(FlatIndex, Quantity);
				}
				else if (!Self->Inventory->DropSlotToWorld(FlatIndex, Quantity, Self->GetOwningPlayerPawn()))
				{
					Self->ShowMessage(TEXT("Inventory_DropBlocked"));
				}
			});
	}
	else if (ActionId == UTAItemActionMenuWidget::ActionDrop)
	{
		const UTAInventorySlotWidget* SlotWidget = SourceSlot.Get();
		if (!SlotWidget || !Inventory)
		{
			return;
		}
		// 丢弃整叠；找不到落点时物品留在原格子
		if (!Inventory->DropSlotToWorld(SlotWidget->GetFlatIndex(), SlotWidget->GetSlotData().Count, GetOwningPlayerPawn()))
		{
			ShowMessage(TEXT("Inventory_DropBlocked"));
		}
	}
}

void UTAInventoryPanelWidget::ShowMessage(const FString& TextId)
{
	FText Message = FText::FromString(TextId);
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Message = Loc->GetText(TextId);
		}
	}
	if (Text_Message)
	{
		Text_Message->SetText(Message);
		Text_Message->SetVisibility(ESlateVisibility::HitTestInvisible);
		MessageTimeLeft = MessageDuration;
	}
	else if (GEngine)
	{
		// 【临时】WBP 里还没有 Text_Message 时的退路
		GEngine->AddOnScreenDebugMessage(-1, MessageDuration, FColor::Orange, Message.ToString());
	}
}

UWidget* UTAInventoryPanelWidget::GetDiaryIconWidget() const
{
	const UTAItemDefinition* Diary = GetDefault<UTADocumentSettings>()->DiaryItem.LoadSynchronous();
	for (const auto& StorySlot : StorySlots)
		if (StorySlot && Diary && StorySlot->GetItemDef() == Diary) return StorySlot->GetIconWidget();
	return nullptr;
}
