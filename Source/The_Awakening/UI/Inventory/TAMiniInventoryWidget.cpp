#include "UI/Inventory/TAMiniInventoryWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TATimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void UTAMiniInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTAMiniInventoryWidget::RefreshLocalizedText);
		}
	}
	RefreshLocalizedText();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTAMiniInventoryWidget::BuildFallbackLayout()
{
	// 【临时】C++ 最简布局：上方“背包已满”，下方格子。WBP_MiniInventory 建好后（需确认后再建）不再使用
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Root"));
	WidgetTree->RootWidget = Root;
	Text_Full = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Full"));
	FSlateFontInfo Font = Text_Full->GetFont();
	Font.Size = 14;
	Text_Full->SetFont(Font);
	if (UVerticalBoxSlot* TextSlot = Root->AddChildToVerticalBox(Text_Full))
	{
		TextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}
	HorizontalBox_Clothing = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Clothing"));
	Root->AddChildToVerticalBox(HorizontalBox_Clothing);
}

void UTAMiniInventoryWidget::NativeDestruct()
{
	BindInventory(nullptr);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAMiniInventoryWidget::RefreshLocalizedText);
		}
	}
	Super::NativeDestruct();
}

void UTAMiniInventoryWidget::BindInventory(UTAInventoryComponent* InInventory)
{
	if (Inventory == InInventory)
	{
		return;
	}
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAMiniInventoryWidget::HandleInventoryUpdated);
	}
	Inventory = InInventory;
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.AddUniqueDynamic(this, &UTAMiniInventoryWidget::HandleInventoryUpdated);
	}
}

void UTAMiniInventoryWidget::ShowPickupResult(UTAInventoryComponent* InInventory, const TArray<int32>& ChangedSlots, bool bInventoryFull)
{
	BindInventory(InInventory);
	if (!Inventory)
	{
		return;
	}
	// 背包已满：只提示，不闪烁；放进去了一部分时照常闪烁放进去的格子
	bShowFull = bInventoryFull;
	BlinkSlots = TSet<int32>(ChangedSlots);
	BlinkElapsed = 0.f;
	TimeLeft = DisplayDuration;
	if (Text_Full)
	{
		Text_Full->SetVisibility(bShowFull ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	// 上一次可能停在渐隐或闪烁暗的一帧
	FadeAlpha = 1.f;
	bBlinkDim = false;
	if (Text_Full)
	{
		Text_Full->SetRenderOpacity(1.f);
	}
	RebuildCellsIfNeeded();
	RefreshCellVisuals();
	UpdateScreenPosition();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTAMiniInventoryWidget::HideNow()
{
	TimeLeft = 0.f;
	BlinkSlots.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTAMiniInventoryWidget::HandleInventoryUpdated()
{
	if (IsShowing())
	{
		RebuildCellsIfNeeded();
		RefreshCellVisuals();
	}
}

void UTAMiniInventoryWidget::RefreshLocalizedText()
{
	if (!Text_Full)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	Text_Full->SetText(Loc ? Loc->GetText(FullTextId) : FText::FromString(FullTextId));
}

void UTAMiniInventoryWidget::RebuildCellsIfNeeded()
{
	if (!Inventory || !HorizontalBox_Clothing || !WidgetTree)
	{
		return;
	}
	TArray<const FTAClothingInstance*> Clothes = { &Inventory->GetInnerClothing() };
	if (Inventory->HasOuterClothing())
	{
		Clothes.Add(&Inventory->GetOuterClothing());
	}
	TArray<FString> LayoutKey;
	for (const FTAClothingInstance* Clothing : Clothes)
	{
		FString Key = GetPathNameSafe(Clothing->Definition);
		for (const FTAPocketRuntime& Pocket : Clothing->Pockets)
		{
			Key += FString::Printf(TEXT("|%d"), Pocket.Slots.Num());
		}
		LayoutKey.Add(Key);
	}
	if (LayoutKey == BuiltLayoutKey && Cells.Num() == Inventory->GetMaxSlots())
	{
		return;
	}
	BuiltLayoutKey = LayoutKey;
	HorizontalBox_Clothing->ClearChildren();
	Cells.Reset();

	// 与背包相同的口袋形状：每件衣服一组，组内每个口袋按 Columns x Rows 摆放，格子顺序与扁平下标一致
	for (const FTAClothingInstance* Clothing : Clothes)
	{
		UHorizontalBox* Group = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* GroupSlot = HorizontalBox_Clothing->AddChildToHorizontalBox(Group))
		{
			GroupSlot->SetPadding(FMargin(0.f, 0.f, PocketSpacing, 0.f));
			GroupSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		const TArray<FTAPocketDef>* PocketDefs = Clothing->Definition ? &Clothing->Definition->Pockets : nullptr;
		for (int32 PocketIndex = 0; PocketIndex < Clothing->Pockets.Num(); ++PocketIndex)
		{
			FTAPocketDef PocketDef;
			if (PocketDefs && PocketDefs->IsValidIndex(PocketIndex))
			{
				PocketDef = (*PocketDefs)[PocketIndex];
			}
			else
			{
				PocketDef.SlotCount = Clothing->Pockets[PocketIndex].Slots.Num();
			}
			UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
			Grid->SetSlotPadding(FMargin(CellSpacing * 0.5f));
			if (UHorizontalBoxSlot* GridSlot = Group->AddChildToHorizontalBox(Grid))
			{
				GridSlot->SetPadding(FMargin(0.f, 0.f, PocketSpacing, 0.f));
				GridSlot->SetVerticalAlignment(VAlign_Bottom);
			}
			for (int32 Index = 0; Index < Clothing->Pockets[PocketIndex].Slots.Num(); ++Index)
			{
				int32 Row = 0;
				int32 Column = 0;
				PocketDef.GetCellPosition(Index, Row, Column);
				UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Grid->AddChildToUniformGrid(Cell, Row, Column);
				Cells.Add(Cell);
			}
		}
	}
}

FSlateBrush UTAMiniInventoryWidget::MakeCellBrush(const FTAInventorySlot& SlotData, float Alpha) const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.ImageSize = FVector2D(CellSize);
	Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.Width = OutlineWidth;
	FLinearColor Outline = OutlineColor;
	Outline.A *= Alpha;
	Brush.OutlineSettings.Color = FSlateColor(Outline);

	FLinearColor Fill = FLinearColor::Transparent;
	if (!SlotData.IsEmpty() && SlotData.ItemDef)
	{
		const int32 MaxStack = SlotData.ItemDef->bCanStack ? FMath::Max(1, SlotData.ItemDef->MaxStack) : 1;
		const float Ratio = FMath::Clamp(static_cast<float>(SlotData.Count) / MaxStack, 0.f, 1.f);
		Fill = Ratio >= 1.f ? FullColor : FLinearColor::LerpUsingHSV(PartialMinColor, FullColor, Ratio);
	}
	Fill.A *= Alpha;
	Brush.TintColor = FSlateColor(Fill);
	return Brush;
}

void UTAMiniInventoryWidget::RefreshCellVisuals()
{
	if (!Inventory)
	{
		return;
	}
	const TArray<FTAInventorySlot>& Slots = Inventory->GetSlots();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (UImage* Cell = Cells[Index])
		{
			const float Alpha = FadeAlpha * (bBlinkDim && BlinkSlots.Contains(Index) ? BlinkDimOpacity : 1.f);
			Cell->SetBrush(MakeCellBrush(Slots.IsValidIndex(Index) ? Slots[Index] : FTAInventorySlot(), Alpha));
		}
	}
}

void UTAMiniInventoryWidget::UpdateScreenPosition()
{
	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	FVector2D ScreenPosition;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Pawn->GetActorLocation() + WorldOffset, ScreenPosition, false))
	{
		SetAlignmentInViewport(ScreenAlignment);
		SetPositionInViewport(ScreenPosition + ScreenOffset, false);
	}
}

void UTAMiniInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsShowing())
	{
		return;
	}
	// 显示时长和闪烁都按游戏时间走：时停、暂停时停住
	const UTATimeSubsystem* Time = UTATimeSubsystem::Get(this);
	const float GameDelta = InDeltaTime * (Time ? Time->GetGameplayTimeScale() : 1.f);
	TimeLeft -= GameDelta;
	if (TimeLeft <= 0.f)
	{
		HideNow();
		return;
	}
	UpdateScreenPosition();

	// 最后 FadeOutDuration 秒整体（格子填充、描边、文字）渐变为透明
	const float NewFadeAlpha = FadeOutDuration > 0.f ? FMath::Clamp(TimeLeft / FadeOutDuration, 0.f, 1.f) : 1.f;
	bool bNewBlinkDim = false;
	if (!BlinkSlots.IsEmpty())
	{
		BlinkElapsed += GameDelta;
		// 每个周期前半段暗、后半段亮，闪完后保持亮
		if (BlinkElapsed >= BlinkPeriod * BlinkCount)
		{
			BlinkSlots.Reset();
		}
		else
		{
			bNewBlinkDim = FMath::Fmod(BlinkElapsed, BlinkPeriod) < BlinkPeriod * 0.5f;
		}
	}
	if (NewFadeAlpha != FadeAlpha || bNewBlinkDim != bBlinkDim)
	{
		FadeAlpha = NewFadeAlpha;
		bBlinkDim = bNewBlinkDim;
		if (Text_Full)
		{
			Text_Full->SetRenderOpacity(FadeAlpha);
		}
		RefreshCellVisuals();
	}
}
