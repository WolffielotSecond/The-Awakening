#include "UI/Inventory/TAClothingSwapWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "Inventory/TAClothingRack.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAInventoryComponent.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TATimeSubsystem.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NamedSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "InputAction.h"
#include "Misc/PackageName.h"

UTAClothingSwapWidget* UTAClothingSwapWidget::Open(APlayerController* PC, ATAClothingRack* InRack, UTAInventoryComponent* InInventory,
	TSubclassOf<UTAClothingSwapWidget> WidgetClass)
{
	if (!PC || !InRack || !InInventory)
	{
		return nullptr;
	}
	UClass* Class = WidgetClass.Get();
	if (!Class && FPackageName::DoesPackageExist(TEXT("/Game/UI/Inventory/WBP_ClothingSwap")))
	{
		Class = LoadClass<UTAClothingSwapWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ClothingSwap.WBP_ClothingSwap_C"));
	}
	if (!Class)
	{
		Class = StaticClass();
	}
	UTAClothingSwapWidget* Widget = CreateWidget<UTAClothingSwapWidget>(PC, Class);
	if (!Widget)
	{
		return nullptr;
	}
	Widget->Init(InRack, InInventory);
	// 与背包（50）同层级之上
	Widget->AddToViewport(55);
	if (AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(PC->GetPawn()))
	{
		Character->NotifyClothingSwapOpened(Widget);
	}
	return Widget;
}

void UTAClothingSwapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
	}
}

void UTAClothingSwapWidget::BuildFallbackLayout()
{
	// 【临时】C++ 最简布局：半透明底 + 左右两列。WBP_ClothingSwap 建好后（需确认后再建）不再使用
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Columns"));
	if (UOverlaySlot* ColumnsSlot = Root->AddChildToOverlay(Columns))
	{
		ColumnsSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnsSlot->SetVerticalAlignment(VAlign_Center);
	}

	auto AddColumn = [&](const TCHAR* Name, TObjectPtr<UTextBlock>& OutTitle) -> UVerticalBox*
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		if (UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Column))
		{
			ColumnSlot->SetPadding(FMargin(48.f, 0.f));
			ColumnSlot->SetVerticalAlignment(VAlign_Center);
		}
		OutTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(OutTitle))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
		return Column;
	};
	auto AddNamedSlot = [&](UPanelWidget* Parent, const TCHAR* Name, TObjectPtr<UNamedSlot>& OutSlot)
	{
		OutSlot = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), Name);
		UPanelSlot* PanelSlot = Parent->AddChild(OutSlot);
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			VSlot->SetHorizontalAlignment(HAlign_Center);
			VSlot->SetPadding(FMargin(0.f, 6.f));
		}
		else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			HSlot->SetVerticalAlignment(VAlign_Center);
			HSlot->SetPadding(FMargin(12.f, 0.f));
		}
	};

	UVerticalBox* RackColumn = AddColumn(TEXT("VerticalBox_Rack"), Text_RackTitle);
	AddNamedSlot(RackColumn, TEXT("NamedSlot_RackCoat"), NamedSlot_RackCoat);
	AddNamedSlot(RackColumn, TEXT("NamedSlot_RackClothing"), NamedSlot_RackClothing);

	UVerticalBox* PlayerColumn = AddColumn(TEXT("VerticalBox_Player"), Text_PlayerTitle);
	AddNamedSlot(PlayerColumn, TEXT("NamedSlot_PlayerCoat"), NamedSlot_PlayerCoat);
	UHorizontalBox* PlayerClothing = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_PlayerClothing"));
	if (UVerticalBoxSlot* ClothingSlot = PlayerColumn->AddChildToVerticalBox(PlayerClothing))
	{
		ClothingSlot->SetHorizontalAlignment(HAlign_Center);
	}
	AddNamedSlot(PlayerClothing, TEXT("NamedSlot_InnerClothing"), NamedSlot_InnerClothing);
	AddNamedSlot(PlayerClothing, TEXT("NamedSlot_OuterClothing"), NamedSlot_OuterClothing);
}

void UTAClothingSwapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (!ConfirmAction)
	{
		ConfirmAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Confirm.IA_Inventory_Confirm"));
	}
	if (!GamepadDragModeAction)
	{
		GamepadDragModeAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Drag.IA_Inventory_Drag"));
	}
	EnsureChildren();
	AcquireGuards();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTAClothingSwapWidget::RefreshLocalizedText);
		}
	}
	RefreshLocalizedText();
	Refresh();
}

void UTAClothingSwapWidget::NativeDestruct()
{
	ReleaseGuards();
	CancelGamepadSelect();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAClothingSwapWidget::RefreshLocalizedText);
		}
	}
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAClothingSwapWidget::HandleInventoryUpdated);
	}
	if (ATAClothingRack* CurrentRack = Rack.Get())
	{
		CurrentRack->OnRackChanged.RemoveDynamic(this, &UTAClothingSwapWidget::HandleRackChanged);
	}
	Super::NativeDestruct();
}

void UTAClothingSwapWidget::RemoveFromParent()
{
	ReleaseGuards();
	CancelGamepadSelect();
	if (AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(GetOwningPlayerPawn()))
	{
		Character->NotifyClothingSwapClosed(this);
	}
	Super::RemoveFromParent();
}

void UTAClothingSwapWidget::AcquireGuards()
{
	if (!InputRequestHandle)
	{
		if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
		{
			FTAInputRequest Request;
			Request.Owner = this;
			Request.Priority = 200;
			Request.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor, ETAInputCapability::Confirm,
				ETAInputCapability::Close, ETAInputCapability::ToggleDrag};
			Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
			Request.Presentation.bShowCursor = true;
			Request.Presentation.Focus = ETAInputFocusRequirement::Target;
			Request.Presentation.FocusTarget = this;
			InputRequestController = PC;
			InputRequestHandle = PC->AcquireInputRequest(Request);
		}
	}
	// 时停：与背包相同，只冻结挂了冻结组件的角色和敌人，UI 照常
	if (!bFreezeRequested)
	{
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
		{
			Time->RequestFreeze(this, 1.f);
			bFreezeRequested = true;
		}
	}
	// 换装期间无敌（不放行任何来源；强制击杀 Damage.Lethal 不受影响）
	if (!ProtectedHealth.IsValid())
	{
		if (UTAHealthComponent* Health = UTAHealthComponent::FindHealthComponent(GetOwningPlayerPawn()))
		{
			InvulnerableHandle = Health->AddProtection(ETAHealthProtection::Invulnerable, this, FGameplayTagContainer());
			ProtectedHealth = Health;
		}
	}
}

void UTAClothingSwapWidget::ReleaseGuards()
{
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
	{
		if (auto* PC = Issuer.Get())
		{
			PC->ReleaseInputRequest(Handle);
		}
	}
	if (bFreezeRequested)
	{
		bFreezeRequested = false;
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
		{
			Time->ReleaseFreeze(this);
		}
	}
	if (UTAHealthComponent* Health = ProtectedHealth.Get())
	{
		Health->RemoveProtection(InvulnerableHandle);
	}
	InvulnerableHandle = FTAHealthProtectionHandle();
	ProtectedHealth.Reset();
}

void UTAClothingSwapWidget::Init(ATAClothingRack* InRack, UTAInventoryComponent* InInventory)
{
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAClothingSwapWidget::HandleInventoryUpdated);
	}
	if (ATAClothingRack* OldRack = Rack.Get())
	{
		OldRack->OnRackChanged.RemoveDynamic(this, &UTAClothingSwapWidget::HandleRackChanged);
	}
	Inventory = InInventory;
	Rack = InRack;
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleInventoryUpdated);
	}
	if (InRack)
	{
		InRack->OnRackChanged.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleRackChanged);
	}
	EnsureChildren();
	Refresh();
}

void UTAClothingSwapWidget::EnsureChildren()
{
	if (!SlotWidgetClass)
	{
		SlotWidgetClass = LoadClass<UTAInventorySlotWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_InventorySlot.WBP_InventorySlot_C"));
		if (!SlotWidgetClass)
		{
			SlotWidgetClass = UTAInventorySlotWidget::StaticClass();
		}
	}
	if (!ClothingPanelClass)
	{
		ClothingPanelClass = LoadClass<UTAClothingPanelWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ClothingPanel.WBP_ClothingPanel_C"));
		if (!ClothingPanelClass)
		{
			ClothingPanelClass = UTAClothingPanelWidget::StaticClass();
		}
	}
	auto SpawnSlotInto = [this](UNamedSlot* Named, TObjectPtr<UTAInventorySlotWidget>& OutSlot)
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
			OutSlot->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleSlotHovered);
			OutSlot->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleSlotUnhovered);
		}
	};
	SpawnSlotInto(NamedSlot_RackCoat, RackCoatSlot);
	SpawnSlotInto(NamedSlot_PlayerCoat, PlayerCoatSlot);

	auto SpawnPanelInto = [this](UNamedSlot* Named, TObjectPtr<UTAClothingPanelWidget>& OutPanel)
	{
		if (!Named || OutPanel)
		{
			return;
		}
		OutPanel = CreateWidget<UTAClothingPanelWidget>(this, ClothingPanelClass);
		if (OutPanel)
		{
			OutPanel->SlotWidgetClass = SlotWidgetClass;
			OutPanel->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleSlotHovered);
			OutPanel->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleSlotUnhovered);
			Named->ClearChildren();
			Named->AddChild(OutPanel);
		}
	};
	SpawnPanelInto(NamedSlot_RackClothing, RackClothingPanel);
	SpawnPanelInto(NamedSlot_InnerClothing, InnerClothingPanel);
	SpawnPanelInto(NamedSlot_OuterClothing, OuterClothingPanel);
	for (UTAClothingPanelWidget* Panel : {RackClothingPanel.Get(), InnerClothingPanel.Get(), OuterClothingPanel.Get()})
	{
		if (Panel)
		{
			Panel->SetInventoryComponent(Inventory);
		}
	}
}

void UTAClothingSwapWidget::Refresh()
{
	EnsureChildren();
	if (!Inventory)
	{
		return;
	}
	const ATAClothingRack* CurrentRack = Rack.Get();
	const FTAClothingInstance RackClothing = CurrentRack ? CurrentRack->GetStoredClothing() : FTAClothingInstance();
	UTAClothingDefinition* WornOuter = Inventory->HasOuterClothing() ? Inventory->GetOuterClothing().Definition.Get() : nullptr;

	if (RackCoatSlot)
	{
		RackCoatSlot->SetEmpty();
		RackCoatSlot->SetClothingDisplay(RackClothing.Definition);
		ConfigureSlot(RackCoatSlot);
	}
	if (PlayerCoatSlot)
	{
		PlayerCoatSlot->SetEmpty();
		PlayerCoatSlot->SetClothingDisplay(WornOuter);
		ConfigureSlot(PlayerCoatSlot);
	}

	// 衣架口袋的格子下标是衣架实例内的扁平下标（从 0 开始），不是背包下标；放下时由 ResolveSlot 区分
	if (RackClothingPanel)
	{
		RackClothingPanel->BuildFromClothing(RackClothing, 0);
		ConfigurePanelSlots(RackClothingPanel);
	}
	if (InnerClothingPanel)
	{
		InnerClothingPanel->BuildFromClothing(Inventory->GetInnerClothing(), 0, WornOuter);
		ConfigurePanelSlots(InnerClothingPanel);
	}
	if (OuterClothingPanel)
	{
		int32 InnerSlots = 0;
		for (const FTAPocketRuntime& Pocket : Inventory->GetInnerClothing().Pockets)
		{
			InnerSlots += Pocket.Slots.Num();
		}
		// 没穿外套时 BuildFromClothing 会清空并折叠面板
		OuterClothingPanel->BuildFromClothing(Inventory->GetOuterClothing(), InnerSlots);
		ConfigurePanelSlots(OuterClothingPanel);
	}
}

void UTAClothingSwapWidget::ConfigureSlot(UTAInventorySlotWidget* InSlot)
{
	if (!InSlot)
	{
		return;
	}
	InSlot->SetInventoryComponent(Inventory);
	TWeakObjectPtr<UTAClothingSwapWidget> WeakThis(this);
	InSlot->SetExternalDragHandlers(
		[WeakThis]() { return WeakThis.IsValid() && WeakThis->AllowsPlayerInput(ETAInputCapability::ToggleDrag); },
		[WeakThis](UTAInventorySlotWidget* Source, UTAInventorySlotWidget* Target)
		{
			return WeakThis.IsValid() && WeakThis->HandleSlotDrop(Source, Target);
		});
}

void UTAClothingSwapWidget::ConfigurePanelSlots(UTAClothingPanelWidget* Panel)
{
	if (!Panel)
	{
		return;
	}
	for (UUserWidget* Widget : Panel->GetGeneratedSlots())
	{
		ConfigureSlot(Cast<UTAInventorySlotWidget>(Widget));
	}
}

UTAClothingSwapWidget::ESlotOwner UTAClothingSwapWidget::ResolveSlot(const UTAInventorySlotWidget* InSlot, int32& OutPocket, int32& OutSlotInPocket) const
{
	OutPocket = INDEX_NONE;
	OutSlotInPocket = INDEX_NONE;
	if (!InSlot)
	{
		return ESlotOwner::None;
	}
	if (InSlot == RackCoatSlot)
	{
		return ESlotOwner::RackCoat;
	}
	if (InSlot == PlayerCoatSlot)
	{
		return ESlotOwner::PlayerCoat;
	}
	auto Contains = [InSlot](const UTAClothingPanelWidget* Panel)
	{
		return Panel && Panel->GetGeneratedSlots().Contains(InSlot);
	};
	if (Contains(RackClothingPanel))
	{
		const ATAClothingRack* CurrentRack = Rack.Get();
		if (!CurrentRack)
		{
			return ESlotOwner::None;
		}
		int32 Remaining = InSlot->GetFlatIndex();
		const TArray<FTAPocketRuntime>& Pockets = CurrentRack->GetStoredClothing().Pockets;
		for (int32 PocketIndex = 0; PocketIndex < Pockets.Num() && Remaining >= 0; ++PocketIndex)
		{
			if (Remaining < Pockets[PocketIndex].Slots.Num())
			{
				OutPocket = PocketIndex;
				OutSlotInPocket = Remaining;
				return ESlotOwner::RackPocket;
			}
			Remaining -= Pockets[PocketIndex].Slots.Num();
		}
		return ESlotOwner::None;
	}
	if (Contains(InnerClothingPanel) || Contains(OuterClothingPanel))
	{
		return ESlotOwner::PlayerPocket;
	}
	return ESlotOwner::None;
}

bool UTAClothingSwapWidget::HandleSlotDrop(UTAInventorySlotWidget* Source, UTAInventorySlotWidget* Target)
{
	if (!Inventory || !Source || !Target || Source == Target)
	{
		return false;
	}
	int32 SourcePocket = INDEX_NONE, SourceSlot = INDEX_NONE, TargetPocket = INDEX_NONE, TargetSlot = INDEX_NONE;
	const ESlotOwner From = ResolveSlot(Source, SourcePocket, SourceSlot);
	const ESlotOwner To = ResolveSlot(Target, TargetPocket, TargetSlot);
	ATAClothingRack* CurrentRack = Rack.Get();

	// 外套格互拖：穿上 / 换下 / 互换
	if ((From == ESlotOwner::RackCoat && To == ESlotOwner::PlayerCoat) || (From == ESlotOwner::PlayerCoat && To == ESlotOwner::RackCoat))
	{
		SwapCoats();
		return true;
	}
	if (From == ESlotOwner::PlayerPocket && To == ESlotOwner::PlayerPocket)
	{
		return Inventory->MoveItemBetweenSlots(Source->GetFlatIndex(), Target->GetFlatIndex());
	}
	if (!CurrentRack)
	{
		return false;
	}
	bool bMoved = false;
	FTAClothingInstance& RackClothing = CurrentRack->GetStoredClothingMutable();
	if (From == ESlotOwner::RackPocket && To == ESlotOwner::RackPocket)
	{
		bMoved = UTAInventoryComponent::MoveSlotContents(RackClothing.Pockets[SourcePocket].Slots[SourceSlot], RackClothing.Pockets[TargetPocket].Slots[TargetSlot]);
	}
	else if (From == ESlotOwner::PlayerPocket && To == ESlotOwner::RackPocket)
	{
		bMoved = Inventory->MoveItemWithExternal(Source->GetFlatIndex(), RackClothing, TargetPocket, TargetSlot, true);
	}
	else if (From == ESlotOwner::RackPocket && To == ESlotOwner::PlayerPocket)
	{
		bMoved = Inventory->MoveItemWithExternal(Target->GetFlatIndex(), RackClothing, SourcePocket, SourceSlot, false);
	}
	if (bMoved)
	{
		CurrentRack->NotifyClothingChanged();
	}
	return bMoved;
}

void UTAClothingSwapWidget::SwapCoats()
{
	ATAClothingRack* CurrentRack = Rack.Get();
	if (!CurrentRack || !Inventory)
	{
		return;
	}
	// 衣架自己记录撤销；身上的外套由 SwapOuter 记录
	CurrentRack->RecordUndo();
	if (Inventory->SwapOuter(CurrentRack->GetStoredClothingMutable()))
	{
		CurrentRack->NotifyClothingChanged();
	}
}

void UTAClothingSwapWidget::HandleInventoryUpdated()
{
	Refresh();
}

void UTAClothingSwapWidget::HandleRackChanged()
{
	Refresh();
}

void UTAClothingSwapWidget::HandleSlotHovered(UTAInventorySlotWidget* InHoveredSlot, UTAItemDefinition* ItemDef)
{
	HoveredSlot = InHoveredSlot;
}

void UTAClothingSwapWidget::HandleSlotUnhovered(UTAInventorySlotWidget* InHoveredSlot)
{
	if (HoveredSlot.Get() == InHoveredSlot)
	{
		HoveredSlot.Reset();
	}
}

void UTAClothingSwapWidget::RefreshLocalizedText()
{
	const UTALocalizeSubsystem* Loc = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Loc = GI->GetSubsystem<UTALocalizeSubsystem>();
	}
	auto Apply = [Loc](UTextBlock* Block, const FString& TextId)
	{
		if (Block)
		{
			Block->SetText(Loc ? Loc->GetText(TextId) : FText::FromString(TextId));
		}
	};
	Apply(Text_RackTitle, RackTitleTextId);
	Apply(Text_PlayerTitle, PlayerTitleTextId);
}

bool UTAClothingSwapWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

TOptional<ETAInputCapability> UTAClothingSwapWidget::ResolvePlayerInput(FKey Key) const
{
	const auto* PC = InputRequestController.Get();
	if (!PC)
	{
		return {};
	}
	if (ConfirmAction && PC->IsKeyMappedToAction(Key, ConfirmAction))
	{
		return ETAInputCapability::Confirm;
	}
	if (GamepadDragModeAction && PC->IsKeyMappedToAction(Key, GamepadDragModeAction))
	{
		return ETAInputCapability::ToggleDrag;
	}
	return {};
}

void UTAClothingSwapWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	if (!AllowsPlayerInput(Capability))
	{
		return;
	}
	if (Capability == ETAInputCapability::Confirm)
	{
		// 手柄确认与背包一致：模拟左键点击；选取中时直接放下
		if (const auto* PC = InputRequestController.Get(); PC && PC->IsSimulatingSyntheticLeftMouseClick())
		{
			return;
		}
		if (GamepadSourceSlot.IsValid())
		{
			ToggleGamepadSelect();
		}
		else
		{
			SimulateLeftMouseClick();
		}
	}
	else if (Capability == ETAInputCapability::ToggleDrag)
	{
		ToggleGamepadSelect();
	}
}

void UTAClothingSwapWidget::ToggleGamepadSelect()
{
	UTAInventorySlotWidget* Source = GamepadSourceSlot.Get();
	UTAInventorySlotWidget* Target = HoveredSlot.Get();
	if (!Source)
	{
		// 第一次按：选中光标所在的非空格子
		if (Target && !Target->IsEmpty() && !Target->IsStorySlot())
		{
			GamepadSourceSlot = Target;
			Target->SetDraggingVisual(true);
		}
		return;
	}
	// 第二次按：放到光标所在格子（与鼠标拖拽同一个处理）
	if (Target && Target != Source)
	{
		HandleSlotDrop(Source, Target);
	}
	CancelGamepadSelect();
}

void UTAClothingSwapWidget::CancelGamepadSelect()
{
	if (UTAInventorySlotWidget* Source = GamepadSourceSlot.Get())
	{
		Source->SetDraggingVisual(false);
	}
	GamepadSourceSlot.Reset();
}

void UTAClothingSwapWidget::SimulateLeftMouseClick()
{
	if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		PC->SimulateSyntheticLeftMouseClick();
	}
}
