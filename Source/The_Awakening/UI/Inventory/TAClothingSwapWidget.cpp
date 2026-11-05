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
#include "Components/NamedSlot.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/BackgroundBlur.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "InputAction.h"
#include "Misc/PackageName.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Core/TAInputIconSubsystem.h"
#include "Components/HorizontalBox.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"

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
	BuildSwapLayout();
}

void UTAClothingSwapWidget::BuildSwapLayout()
{
	// A separate runtime tree prevents the old centered Blueprint columns from
	// reflowing when a coat/image disappears. Input and inventory ownership stay unchanged.
	WidgetTree = NewObject<UWidgetTree>(this, TEXT("ClothingSwapLayoutTree"));
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_ClothingSwap"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;
	auto Fill = [&](UWidget* Widget)
	{
		auto* Slot = Root->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		Slot->SetOffsets(FMargin(0.f));
	};
	auto* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("BackgroundBlur_Swap"));
	Blur->SetBlurStrength(.4f);
	Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Blur);
	auto* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Dim"));
	Dim->SetBrushColor(FLinearColor(FColor(0x10, 0x18, 0x20)).CopyWithNewOpacity(.5f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Dim);
	auto Title = [&](const TCHAR* Name, float X)
	{
		auto* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.FontObject = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/TA_GlobalFont.TA_GlobalFont"));
		Font.Size = 28;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(FColor(0xB9, 0xD7, 0xD8))));
		auto* Slot = Root->AddChildToCanvas(Text);
		Slot->SetAnchors(FAnchors(X, .045f));
		Slot->SetAlignment(FVector2D(.5f, 0.f));
		Slot->SetAutoSize(true);
		return Text;
	};
	Text_RackTitle = Title(TEXT("Text_RackTitle"), .25f);
	Text_PlayerTitle = Title(TEXT("Text_PlayerTitle"), .75f);
	auto Coat = [&](const TCHAR* Name, float X)
	{
		auto* Named = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), Name);
		auto* Slot = Root->AddChildToCanvas(Named);
		Slot->SetAnchors(FAnchors(X, .13f));
		Slot->SetAlignment(FVector2D(.5f, 0.f));
		Slot->SetSize(FVector2D(80.f, 80.f));
		return Named;
	};
	NamedSlot_RackCoat = Coat(TEXT("NamedSlot_RackCoat"), .25f);
	NamedSlot_PlayerCoat = Coat(TEXT("NamedSlot_PlayerCoat"), .75f);
	auto Body = [&](const TCHAR* Name, float Left, float Right)
	{
		auto* Named = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), Name);
		auto* Slot = Root->AddChildToCanvas(Named);
		Slot->SetAnchors(FAnchors(Left, .28f, Right, .91f));
		Slot->SetOffsets(FMargin(0.f));
		return Named;
	};
	NamedSlot_RackClothing = Body(TEXT("NamedSlot_RackClothing"), .035f, .485f);
	NamedSlot_InnerClothing = Body(TEXT("NamedSlot_InnerClothing"), .515f, .965f);
	NamedSlot_OuterClothing = nullptr; // The right side renders inner + outer in one panel.
	ActionPromptBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Controls"));
	auto* PromptPlacement = Root->AddChildToCanvas(ActionPromptBar);
	PromptPlacement->SetAnchors(FAnchors(.965f, .96f));
	PromptPlacement->SetAlignment(FVector2D(1.f, 1.f));
	PromptPlacement->SetAutoSize(true);
	PromptPlacement->SetZOrder(10);
}

void UTAClothingSwapWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	UpdateGamepadDragVisualPosition();
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.f || Size.Y <= 0.f) return;
	const float Extent = FMath::Clamp(float(FMath::Min(Size.X * .06f, Size.Y * .09f)), 36.f, 100.f);
	for (UNamedSlot* Named : { NamedSlot_RackCoat.Get(), NamedSlot_PlayerCoat.Get() })
		if (Named)
			if (auto* Placement = Cast<UCanvasPanelSlot>(Named->Slot))
				if (!Placement->GetSize().Equals(FVector2D(Extent, Extent), .1f)) Placement->SetSize(FVector2D(Extent, Extent));
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
	if (!BackAction)
	{
		BackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_UIBack.IA_UIBack"));
	}
	if (!ContextAction) ContextAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_ContextAction.IA_Inventory_ContextAction"));
	EnsureChildren();
	AcquireGuards();
	if (UGameInstance* GI = GetGameInstance())
	{
		InputIconSubsystem = GI->GetSubsystem<UTAInputIconSubsystem>();
		if (InputIconSubsystem)
		{
			InputIconSubsystem->OnInputPromptsChanged.AddUniqueDynamic(this, &UTAClothingSwapWidget::RefreshInputPrompts);
		}
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTAClothingSwapWidget::RefreshLocalizedText);
		}
	}
	BuildActionPromptBar();
	RefreshInputPrompts();
	RefreshLocalizedText();
	Refresh();
}

void UTAClothingSwapWidget::NativeDestruct()
{
	ReleaseGuards();
	CancelGamepadSelect();
	if (InputIconSubsystem)
	{
		InputIconSubsystem->OnInputPromptsChanged.RemoveDynamic(this, &UTAClothingSwapWidget::RefreshInputPrompts);
		InputIconSubsystem = nullptr;
	}
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
			for (UInputAction* Action : {ConfirmAction.Get(), GamepadDragModeAction.Get(), ContextAction.Get(), BackAction.Get()})
				PC->RegisterEnhancedUIAction(Action);
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
			if (auto* Frame = Cast<UBorder>(OutSlot->GetWidgetFromName(TEXT("Border"))))
			{
				FSlateBrush Brush = Frame->Background;
				Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Textures/Inventory/Grid_Solid.Grid_Solid")));
				Frame->SetBrush(Brush);
			}
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
			if (Named == NamedSlot_RackClothing) OutPanel->SetRackSwapLayout();
			else OutPanel->SetWornInventoryLayout();
			OutPanel->SlotWidgetClass = SlotWidgetClass;
			OutPanel->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleSlotHovered);
			OutPanel->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleSlotUnhovered);
			Named->ClearChildren();
			Named->AddChild(OutPanel);
		}
	};
	SpawnPanelInto(NamedSlot_RackClothing, RackClothingPanel);
	SpawnPanelInto(NamedSlot_InnerClothing, InnerClothingPanel);
	for (UTAClothingPanelWidget* Panel : {RackClothingPanel.Get(), InnerClothingPanel.Get()})
	{
		if (Panel)
		{
			Panel->SetInventoryComponent(Inventory);
		}
	}
}

void UTAClothingSwapWidget::Refresh()
{
	// Inventory updates may rebuild pocket widgets or replace the selected coat.
	CancelGamepadSelect();
	HoveredSlot.Reset();
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
		RackClothingPanel->BuildRackClothing(RackClothing);
		ConfigurePanelSlots(RackClothingPanel);
	}
	if (InnerClothingPanel)
	{
		InnerClothingPanel->BuildWornClothing(Inventory->GetInnerClothing(), Inventory->GetOuterClothing());
		ConfigurePanelSlots(InnerClothingPanel);
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
	if (Contains(InnerClothingPanel))
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
	for (UTAActionPromptWidget* Prompt : ActionPrompts)
	{
		if (Prompt) Prompt->RefreshPrompt();
	}
}

void UTAClothingSwapWidget::BuildActionPromptBar()
{
	if (!ActionPromptBar || !ActionPrompts.IsEmpty()) return;
	auto Add = [this](UInputAction* Action, const TCHAR* TextId)
	{
		if (auto* Prompt = FTAPromptWidgetUtils::AddActionPrompt(this, ActionPromptBar, nullptr, Action, TextId,
			FLinearColor(FColor(0xB9, 0xD7, 0xD8))))
		{
			Prompt->OnPromptClicked.AddUniqueDynamic(this, &UTAClothingSwapWidget::HandleActionPromptClicked);
			ActionPrompts.Add(Prompt);
		}
	};
	Add(ConfirmAction, TEXT("UI_Inventory_Confirm"));
	Add(GamepadDragModeAction, TEXT("UI_Inventory_DragMode"));
	Add(ContextAction, TEXT("UI_Inventory_ContextBack"));
	Add(BackAction, TEXT("UI_Document_Back"));
}

void UTAClothingSwapWidget::RefreshInputPrompts()
{
	const bool bGamepad = InputIconSubsystem && InputIconSubsystem->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
	for (UTAActionPromptWidget* Prompt : ActionPrompts)
	{
		if (!Prompt) continue;
		const bool bPointerAction = Prompt->GetPromptAction() == ConfirmAction || Prompt->GetPromptAction() == GamepadDragModeAction;
		const bool bHide = (bPointerAction && !bGamepad) || (Prompt->GetPromptAction() == ContextAction && bGamepad);
		Prompt->SetVisibility(bHide ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Prompt->RefreshPrompt();
	}
}

void UTAClothingSwapWidget::HandleActionPromptClicked(UTAActionPromptWidget* Prompt)
{
	if (!Prompt) return;
	if (Prompt->GetPromptAction() == ContextAction) HandleContextAction();
	else if (Prompt->GetPromptAction() == BackAction)
	{
		if (AllowsPlayerInput(ETAInputCapability::Close)) Close();
	}
	else if (Prompt->GetPromptAction() == ConfirmAction)
	{
		HandleConfirmPressed();
	}
	else if (Prompt->GetPromptAction() == GamepadDragModeAction)
	{
		if (AllowsPlayerInput(ETAInputCapability::ToggleDrag)) ToggleGamepadSelect();
	}
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
	// Physical keys execute only through evaluated IMC action events.
}

void UTAClothingSwapWidget::ExecutePlayerAction(const UInputAction* Action, ETriggerEvent Event, const FInputActionInstance& Instance)
{
	if (!Action || Event != ETriggerEvent::Started) return;
	if (Action == BackAction && AllowsPlayerInput(ETAInputCapability::Close)) Close();
	else if (Action == ContextAction) HandleContextAction();
	else if (Action == ConfirmAction) HandleConfirmPressed();
	else if (Action == GamepadDragModeAction && AllowsPlayerInput(ETAInputCapability::ToggleDrag)) ToggleGamepadSelect();
}

void UTAClothingSwapWidget::HandleContextAction()
{
	if (!AllowsPlayerInput(ETAInputCapability::Close) || GamepadSourceSlot.IsValid() || UWidgetBlueprintLibrary::IsDragDropping()) return;
	const UTAInventorySlotWidget* CurrentSlot = HoveredSlot.Get();
	if (!CurrentSlot || CurrentSlot->IsEmpty()) Close();
}

void UTAClothingSwapWidget::HandleConfirmPressed()
{
	if (!AllowsPlayerInput(ETAInputCapability::Confirm)) return;
	if (const auto* PC = InputRequestController.Get(); PC && PC->IsSimulatingSyntheticLeftMouseClick()) return;
	if (GamepadSourceSlot.IsValid()) ToggleGamepadSelect();
	else SimulateLeftMouseClick();
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
			GamepadDragVisualWidget = CreateWidget<UTAInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			if (!GamepadDragVisualWidget) return;
			GamepadDragVisualWidget->SetSlotData(Target->GetSlotData(), Target->GetFlatIndex());
			GamepadDragVisualWidget->SetClothingDisplay(Target->GetClothingDisplay());
			GamepadDragVisualWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			GamepadDragVisualWidget->AddToViewport(1000001);
			GamepadDragVisualWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
			GamepadSourceSlot = Target;
			Target->SetDraggingVisual(true);
			UpdateGamepadDragVisualPosition();
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
	if (GamepadDragVisualWidget)
	{
		GamepadDragVisualWidget->RemoveFromParent();
		GamepadDragVisualWidget = nullptr;
	}
}

void UTAClothingSwapWidget::UpdateGamepadDragVisualPosition()
{
	if (!GamepadDragVisualWidget) return;
	if (!GamepadSourceSlot.IsValid())
	{
		CancelGamepadSelect();
		return;
	}
	auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
	float X = 0.f, Y = 0.f;
	if (!PC || !PC->GetPlayerCursorPosition(X, Y)) return;
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PC), KINDA_SMALL_NUMBER);
	GamepadDragVisualWidget->SetPositionInViewport(FVector2D(X / Scale + 14.f, Y / Scale + 14.f), false);
}

void UTAClothingSwapWidget::SimulateLeftMouseClick()
{
	if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		PC->SimulateSyntheticLeftMouseClick();
	}
}
