#include "UI/Inventory/TAClothingPanelWidget.h"
#include "Inventory/TAItemSettings.h"
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
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "UI/Inventory/TAWornClothingLayout.h"

TSharedRef<SWidget> UTAClothingPanelWidget::RebuildWidget()
{
	if (bWornInventoryLayout)
	{
		if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
		CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WornClothingCanvas"));
		WidgetTree->RootWidget = CanvasRoot;
		CanvasRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* Picture = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("WornClothingPicture"));
		Picture->SetStretch(EStretch::ScaleToFit);
		Picture->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* PictureSlot = CanvasRoot->AddChildToCanvas(Picture);
		PictureSlot->SetAnchors(FAnchors(0.f, 0.f, .52f, 1.f));
		PictureSlot->SetOffsets(FMargin(0.f));
		Image_Clothing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Clothing"));
		Picture->AddChild(Image_Clothing);
		auto MakeFrame = [&](const TCHAR* Name)
		{
			auto* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
			FSlateBrush Brush;
			Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Textures/Inventory/Grid.Grid")));
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = FVector2D(512.f, 512.f);
			Brush.Margin = FMargin(2.f / 512.f);
			Frame->SetBrush(Brush);
			Frame->SetBrushColor(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("799FA7"))));
			Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
			CanvasRoot->AddChildToCanvas(Frame);
			return Frame;
		};
		InnerFrame = MakeFrame(TEXT("InnerPocketFrame"));
		OuterFrame = MakeFrame(TEXT("OuterPocketFrame"));
		BuiltDef = nullptr;
		GeneratedGroups.Reset(); GeneratedSlots.Reset(); WornCells.Reset(); WornShapes.Reset();
		LastWornSize = FVector2D::ZeroVector;
	}
	return Super::RebuildWidget();
}

void UTAClothingPanelWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (bWornInventoryLayout && !Geometry.GetLocalSize().Equals(LastWornSize))
	{
		ArrangeWornClothing(Geometry.GetLocalSize());
	}
}

void UTAClothingPanelWidget::CreateWornPocket(const FTAPocketRuntime& Runtime, const FTAPocketDef& Def, int32& FlatIndex)
{
	int32 Columns, Rows;
	Def.GetGridSize(Columns, Rows);
	// Preserve every runtime slot if an old save predates a changed definition.
	Rows = FMath::Max(Rows, FMath::DivideAndRoundUp(Runtime.Slots.Num(), FMath::Max(1, Columns)));
	WornShapes.Add(FIntPoint(Columns, Rows));
	auto* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	CanvasRoot->AddChildToCanvas(Grid)->SetZOrder(1);
	GeneratedGroups.Add(Grid);
	for (int32 Index = 0; Index < Runtime.Slots.Num(); ++Index)
	{
		auto* Cell = WidgetTree->ConstructWidget<USizeBox>();
		auto* SlotWidget = CreateWidget<UUserWidget>(this, SlotWidgetClass);
		Cell->AddChild(SlotWidget);
		Grid->AddChildToUniformGrid(Cell, Index / FMath::Max(1, Columns), Index % FMath::Max(1, Columns));
		if (auto* InvSlot = Cast<UTAInventorySlotWidget>(SlotWidget))
		{
			InvSlot->SetInventoryComponent(Inventory);
			InvSlot->SetInputOwner(GetTypedOuter<UTAInventoryPanelWidget>());
			InvSlot->SetSlotData(Runtime.Slots[Index], FlatIndex);
			InvSlot->OnInventorySlotHovered.AddUniqueDynamic(this, &UTAClothingPanelWidget::HandleSlotHovered);
			InvSlot->OnInventorySlotUnhovered.AddUniqueDynamic(this, &UTAClothingPanelWidget::HandleSlotUnhovered);
		}
		GeneratedSlots.Add(SlotWidget);
		WornCells.Add(Cell);
		++FlatIndex;
	}
}

void UTAClothingPanelWidget::BuildWornClothing(const FTAClothingInstance& Inner, const FTAClothingInstance& Outer)
{
	if (!bWornInventoryLayout) return;
	TakeWidget(); // Build the native canvas before filling its groups (Init can precede Construct).
	const bool bHasOuter = Outer.IsValid();
	UTAClothingDefinition* Picture = bHasOuter ? Outer.Definition.Get() : Inner.Definition.Get();
	SetClothingIcon(Picture);
	TArray<FIntPoint> Shapes;
	int32 TotalSlots = 0;
	auto Collect = [&](const FTAClothingInstance& Instance)
	{
		for (int32 Index = 0; Index < Instance.Pockets.Num(); ++Index)
		{
			FTAPocketDef Def;
			if (Instance.Definition && Instance.Definition->Pockets.IsValidIndex(Index)) Def = Instance.Definition->Pockets[Index];
			int32 C, R; Def.GetGridSize(C, R);
			R = FMath::Max(R, FMath::DivideAndRoundUp(Instance.Pockets[Index].Slots.Num(), FMath::Max(1, C)));
			Shapes.Add(FIntPoint(C, R));
			TotalSlots += Instance.Pockets[Index].Slots.Num();
		}
	};
	Collect(Inner);
	if (bHasOuter) Collect(Outer);
	const bool bSame = BuiltDef == Inner.Definition && WornOuterDef == Outer.Definition &&
		InnerPocketCount == Inner.Pockets.Num() && Shapes == WornShapes && GeneratedSlots.Num() == TotalSlots;
	if (!bSame)
	{
		ClearPanel();
		WornCells.Reset(); WornShapes.Reset();
		BuiltDef = Inner.Definition;
		WornOuterDef = Outer.Definition;
		InnerPocketCount = Inner.Pockets.Num();
		int32 FlatIndex = 0;
		auto Build = [&](const FTAClothingInstance& Instance)
		{
			for (int32 Index = 0; Index < Instance.Pockets.Num(); ++Index)
			{
				FTAPocketDef Def;
				if (Instance.Definition && Instance.Definition->Pockets.IsValidIndex(Index)) Def = Instance.Definition->Pockets[Index];
				CreateWornPocket(Instance.Pockets[Index], Def, FlatIndex);
			}
		};
		Build(Inner);
		if (bHasOuter) Build(Outer);
		LastWornSize = FVector2D::ZeroVector;
	}
	else
	{
		int32 Index = 0;
		auto Update = [&](const FTAClothingInstance& Instance)
		{
			for (const FTAPocketRuntime& Pocket : Instance.Pockets)
				for (const FTAInventorySlot& Slot : Pocket.Slots)
				{
					if (auto* UI = Cast<UTAInventorySlotWidget>(GeneratedSlots[Index])) UI->SetSlotData(Slot, Index);
					++Index;
				}
		};
		Update(Inner); if (bHasOuter) Update(Outer);
	}
	OuterFrame->SetVisibility(bHasOuter ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ArrangeWornClothing(GetCachedGeometry().GetLocalSize());
}

void UTAClothingPanelWidget::ArrangeWornClothing(FVector2D Size)
{
	if (Size.X <= 0.f || Size.Y <= 0.f || !InnerFrame) return;
	LastWornSize = Size;
	TArray<FIntPoint> Inner, Outer;
	for (int32 Index = 0; Index < WornShapes.Num(); ++Index)
		(Index < InnerPocketCount ? Inner : Outer).Add(WornShapes[Index]);
	const FVector2D Origin(Size.X * .60f, 0.f);
	const auto Layout = TAWornClothingLayout::Calculate(FVector2D(Size.X * .40f, Size.Y), Inner, Outer);
	auto Position = [&](UWidget* Widget, FVector2D At, FVector2D Extent)
	{
		auto* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
		Slot->SetPosition(Origin + At); Slot->SetSize(Extent);
	};
	Position(InnerFrame, Layout.Inner.Position, Layout.Inner.Size);
	Position(OuterFrame, Layout.Outer.Position, Layout.Outer.Size);
	for (int32 Index = 0; Index < GeneratedGroups.Num(); ++Index)
	{
		const bool bInner = Index < InnerPocketCount;
		const auto& Group = bInner ? Layout.Inner : Layout.Outer;
		Position(GeneratedGroups[Index], Group.Position + Group.Pockets[bInner ? Index : Index - InnerPocketCount],
			FVector2D(WornShapes[Index].X * Layout.Cell, WornShapes[Index].Y * Layout.Cell));
	}
	for (USizeBox* Cell : WornCells)
	{
		Cell->SetWidthOverride(Layout.Cell); Cell->SetHeightOverride(Layout.Cell);
	}
}

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
	BuiltDisplayOverride = nullptr;
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
		// 没有图标时用项目设置里的默认图标
		Icon = UTAItemSettings::ResolveIcon(Icon);
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

	// 每个口袋按自己的 Columns x Rows 网格摆放；旧资产没有形状时沿用原来的两列 / 两行摆法
	for (int32 i = 0; i < Num; ++i)
	{
		int32 Row = 0;
		int32 Col = 0;
		PocketDef.GetCellPosition(i, Row, Col);

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

void UTAClothingPanelWidget::BuildFromClothing(const FTAClothingInstance& Instance, int32 FlatIndexStart, const UTAClothingDefinition* DisplayOverride)
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
		BuiltDisplayOverride.Get() == DisplayOverride &&
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
	BuiltDisplayOverride = DisplayOverride;
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
		if (DisplayOverride && DisplayOverride->bOverrideInnerPocketDisplay && DisplayOverride->InnerPocketDisplayOverrides.IsValidIndex(p))
		{
			const FTAPocketDisplayOverride& Override = DisplayOverride->InnerPocketDisplayOverrides[p];
			PocketDef.AnchorUV = Override.AnchorUV;
			PocketDef.SlotSide = Override.SlotSide;
		}
		CreateSlotsForPocket(Instance.Pockets[p], PocketDef, FlatIndex);
	}
}
