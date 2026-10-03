#include "UI/Inventory/TAClothingPanelWidget.h"
#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "Inventory/TAClothingDefinition.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"

void UTAClothingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!SlotWidgetClass)
	{
		SlotWidgetClass = UTAInventorySlotWidget::StaticClass();
	}
	EnsureBindings();
}

void UTAClothingPanelWidget::EnsureBindings()
{
	if (!CanvasRoot)
	{
		CanvasRoot = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("CanvasRoot")));
	}
	if (!Image_Clothing)
	{
		Image_Clothing = Cast<UImage>(GetWidgetFromName(TEXT("Image_Clothing")));
	}
	if (!Box_Top)
	{
		Box_Top = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("Box_Top")));
	}
	if (!Box_Bottom)
	{
		Box_Bottom = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("Box_Bottom")));
	}
	if (!Box_Left)
	{
		Box_Left = Cast<UVerticalBox>(GetWidgetFromName(TEXT("Box_Left")));
	}
	if (!Box_Right)
	{
		Box_Right = Cast<UVerticalBox>(GetWidgetFromName(TEXT("Box_Right")));
	}
}

void UTAClothingPanelWidget::ClearPanel()
{
	for (UWidget* Group : GeneratedGroups)
	{
		if (Group)
		{
			Group->RemoveFromParent();
		}
	}
	GeneratedGroups.Reset();
	GeneratedSlots.Reset();

	CurrentDef = nullptr;
	BuiltDef = nullptr;
	BuiltFlatStart = INDEX_NONE;
}

void UTAClothingPanelWidget::SetClothingIcon(UTAClothingDefinition* Def)
{
	EnsureBindings();
	if (!Image_Clothing)
	{
		return;
	}

	UTexture2D* Icon = nullptr;
	if (Def)
	{
		// TSoftObjectPtr：
		if (!Def->Icon.IsNull())
		{
			Icon = Def->Icon.LoadSynchronous();
		}
		// TObjectPtr 则改为：Icon = Def->Icon.Get();
	}

	if (Icon)
	{
		Image_Clothing->SetBrushFromTexture(Icon);
		Image_Clothing->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Image_Clothing->SetVisibility(ESlateVisibility::Hidden);
	}
}

UPanelWidget* UTAClothingPanelWidget::GetBoxForSide(ETAPocketSlotSide Side) const
{
	switch (Side)
	{
	case ETAPocketSlotSide::Top:    return Box_Top;
	case ETAPocketSlotSide::Bottom: return Box_Bottom;
	case ETAPocketSlotSide::Left:   return Box_Left;
	case ETAPocketSlotSide::Right:  return Box_Right;
	default: return Box_Right;
	}
}

void UTAClothingPanelWidget::CreateSlotsForPocket(
	const FTAPocketRuntime& PocketRuntime,
	const FTAPocketDef& PocketDef,
	int32& InOutFlatIndex)
{
	EnsureBindings();
	if (!SlotWidgetClass || !WidgetTree)
	{
		return;
	}

	UPanelWidget* SideBox = GetBoxForSide(PocketDef.SlotSide);
	if (!SideBox)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSlotsForPocket: Side box null. Check Box_Top/Bottom/Left/Right names."));
		return;
	}

	const int32 Num = PocketRuntime.Slots.Num();
	if (Num <= 0)
	{
		return;
	}

	// 每个口袋一个组，避免不同组混排
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	if (!Grid)
	{
		return;
	}

	SideBox->AddChild(Grid);

	// 居中
	if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(Grid->Slot))
	{
		// Top / Bottom
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HS->SetHorizontalAlignment(HAlign_Center);
		HS->SetVerticalAlignment(VAlign_Center);
		HS->SetPadding(FMargin(4.f));
	}
	else if (UVerticalBoxSlot* VS = Cast<UVerticalBoxSlot>(Grid->Slot))
	{
		// Left / Right
		VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VS->SetHorizontalAlignment(HAlign_Center);
		VS->SetVerticalAlignment(VAlign_Center);
		VS->SetPadding(FMargin(4.f));
	}

	GeneratedGroups.Add(Grid);

	const bool bTwoColumns = (PocketDef.SlotSide == ETAPocketSlotSide::Left
		|| PocketDef.SlotSide == ETAPocketSlotSide::Right);
	// Left/Right：两列（行优先，最后一行可 1 格）
	// Top/Bottom：两行（列优先，最后一列可 1 格）

	for (int32 i = 0; i < Num; ++i)
	{
		int32 Row = 0;
		int32 Col = 0;
		if (bTwoColumns)
		{
			Row = i / 2;
			Col = i % 2;
		}
		else
		{
			Col = i / 2;
			Row = i % 2;
		}

		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(SlotSize);
		Cell->SetHeightOverride(SlotSize);

		UUserWidget* SlotWidget = CreateWidget<UUserWidget>(this, SlotWidgetClass);
		if (SlotWidget)
		{
			Cell->AddChild(SlotWidget);
			if (UTAInventorySlotWidget* InvSlot = Cast<UTAInventorySlotWidget>(SlotWidget))
			{
				InvSlot->SetInventoryComponent(Inventory);
				// Nested slots belong to this panel's Inventory owner, never the current winner.
				InvSlot->SetInputOwner(GetTypedOuter<UTAInventoryPanelWidget>());
				InvSlot->SetSlotData(PocketRuntime.Slots[i], InOutFlatIndex);
				InvSlot->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAClothingPanelWidget::HandleSlotHovered);
				InvSlot->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAClothingPanelWidget::HandleSlotUnhovered);
			}
			GeneratedSlots.Add(SlotWidget);
		}

		Grid->AddChildToUniformGrid(Cell, Row, Col);
		if (UUniformGridSlot* GS = Cast<UUniformGridSlot>(Cell->Slot))
		{
			GS->SetHorizontalAlignment(HAlign_Center);
			GS->SetVerticalAlignment(VAlign_Center);
		}

		++InOutFlatIndex;
	}
}

void UTAClothingPanelWidget::HandleSlotHovered(UTAInventorySlotWidget* HoveredSlot, UTAItemDefinition* ItemDef)
{
	OnInventorySlotHovered.Broadcast(HoveredSlot, ItemDef);
}

void UTAClothingPanelWidget::HandleSlotUnhovered(UTAInventorySlotWidget* HoveredSlot)
{
	OnInventorySlotUnhovered.Broadcast(HoveredSlot);
}

void UTAClothingPanelWidget::BuildFromClothing(const FTAClothingInstance& Instance, int32 FlatIndexStart)
{
	EnsureBindings();

	if (!Instance.Definition)
	{
		ClearPanel();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::Visible);

	int32 TotalRuntimeSlots = 0;
	for (const FTAPocketRuntime& P : Instance.Pockets)
	{
		TotalRuntimeSlots += P.Slots.Num();
	}

	const bool bSameLayout =
		BuiltDef == Instance.Definition &&
		BuiltFlatStart == FlatIndexStart &&
		GeneratedSlots.Num() > 0 &&
		GeneratedSlots.Num() == TotalRuntimeSlots;

	if (bSameLayout)
	{
		int32 Idx = 0;
		int32 Flat = FlatIndexStart;
		for (const FTAPocketRuntime& Pocket : Instance.Pockets)
		{
			for (const FTAInventorySlot& SlotData : Pocket.Slots)
			{
				if (GeneratedSlots.IsValidIndex(Idx))
				{
					if (UTAInventorySlotWidget* InvSlot = Cast<UTAInventorySlotWidget>(GeneratedSlots[Idx]))
					{
						InvSlot->SetSlotData(SlotData, Flat);
					}
				}
				++Idx;
				++Flat;
			}
		}
		CurrentDef = Instance.Definition;
		return;
	}

	ClearPanel();
	BuiltDef = Instance.Definition;
	BuiltFlatStart = FlatIndexStart;
	CurrentDef = Instance.Definition;
	SetClothingIcon(CurrentDef);

	int32 FlatIndex = FlatIndexStart;
	const TArray<FTAPocketDef>& PocketDefs = CurrentDef->Pockets;

	for (int32 p = 0; p < Instance.Pockets.Num(); ++p)
	{
		FTAPocketDef PocketDef;
		if (PocketDefs.IsValidIndex(p))
		{
			PocketDef = PocketDefs[p];
		}
		else
		{
			PocketDef.PocketId = Instance.Pockets[p].PocketId;
			PocketDef.SlotCount = Instance.Pockets[p].Slots.Num();
			PocketDef.AnchorUV = FVector2D(0.5f, 0.5f);
			PocketDef.SlotSide = ETAPocketSlotSide::Right;
		}
		CreateSlotsForPocket(Instance.Pockets[p], PocketDef, FlatIndex);
	}
}
