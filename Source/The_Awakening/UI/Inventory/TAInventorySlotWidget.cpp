#include "UI/Inventory/TAInventorySlotWidget.h"
#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Inventory/TAInventoryComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Engine/Texture2D.h"

void UTAInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshVisuals();
}

FReply UTAInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!AllowsPlayerDrag()) return FReply::Handled();
	if (!IsEmpty() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTAInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	OnInventorySlotHovered.Broadcast(this, IsEmpty() ? nullptr : CachedSlot.ItemDef);
}

void UTAInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnInventorySlotUnhovered.Broadcast(this);
}

void UTAInventorySlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	if (!AllowsPlayerDrag()) return;
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (IsEmpty() || !Inventory)
	{
		return;
	}

	UDragDropOperation* DragOperation = UWidgetBlueprintLibrary::CreateDragDropOperation(UDragDropOperation::StaticClass());
	if (!DragOperation)
	{
		return;
	}

	DragOperation->Payload = this;
	DragOperation->Pivot = EDragPivot::MouseDown;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UTAInventorySlotWidget* DragVisual = CreateWidget<UTAInventorySlotWidget>(PC, GetClass()))
		{
			DragVisual->SetSlotData(CachedSlot, FlatIndex);
			DragVisual->SetInventoryComponent(Inventory);
			DragOperation->DefaultDragVisual = DragVisual;
		}
	}

	OutOperation = DragOperation;
	SetDraggingVisual(true);
}

void UTAInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDraggingVisual(false);
	if (IsHovered())
	{
		OnInventorySlotHovered.Broadcast(this, IsEmpty() ? nullptr : CachedSlot.ItemDef);
	}
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

bool UTAInventorySlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	UTAInventorySlotWidget* SourceSlot = InOperation ? Cast<UTAInventorySlotWidget>(InOperation->Payload) : nullptr;
	if (SourceSlot) SourceSlot->SetDraggingVisual(false); // cleanup is never permission-gated
	if (!AllowsPlayerDrag() || !SourceSlot || !SourceSlot->AllowsPlayerDrag()) return false;
	if (!SourceSlot || SourceSlot == this || !Inventory || SourceSlot->Inventory != Inventory)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	SourceSlot->SetDraggingVisual(false);
	const bool bMoved = Inventory->MoveItemBetweenSlots(SourceSlot->FlatIndex, FlatIndex);
	return bMoved || Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UTAInventorySlotWidget::SetInputOwner(UTAInventoryPanelWidget* InOwner)
{
	InputOwner = InOwner;
}

bool UTAInventorySlotWidget::AllowsPlayerDrag() const
{
	return InputOwner.IsValid() && InputOwner->AllowsPlayerInput(ETAInputCapability::ToggleDrag);
}

void UTAInventorySlotWidget::SetSlotData(const FTAInventorySlot& SlotData, int32 InFlatIndex)
{
	CachedSlot = SlotData;
	FlatIndex = InFlatIndex;
	RefreshVisuals();
	if (IsHovered())
	{
		OnInventorySlotHovered.Broadcast(this, IsEmpty() ? nullptr : CachedSlot.ItemDef);
	}
}

void UTAInventorySlotWidget::SetEmpty()
{
	CachedSlot = FTAInventorySlot();
	FlatIndex = INDEX_NONE;
	RefreshVisuals();
	if (IsHovered())
	{
		OnInventorySlotHovered.Broadcast(this, nullptr);
	}
}

void UTAInventorySlotWidget::SetDraggingVisual(bool bDragging)
{
	bIsDragging = bDragging;
	const float Opacity = bIsDragging ? 0.5f : 1.0f;
	if (Image_item)
	{
		Image_item->SetRenderOpacity(Opacity);
	}
	if (Text_Count)
	{
		Text_Count->SetRenderOpacity(Opacity);
	}
	if (Text_Name)
	{
		Text_Name->SetRenderOpacity(Opacity);
	}
}

bool UTAInventorySlotWidget::IsEmpty() const
{
	return CachedSlot.IsEmpty();
}

UTAItemDefinition* UTAInventorySlotWidget::GetItemDef() const
{
	return CachedSlot.ItemDef;
}

void UTAInventorySlotWidget::RefreshVisuals()
{
	const bool bHasItem = !CachedSlot.IsEmpty() && CachedSlot.ItemDef;

	if (Image_item)
	{
		if (bHasItem)
		{
			UTexture2D* Icon = CachedSlot.ItemDef->Icon.Get(); // 若是 Soft 则 LoadSynchronous
			if (Icon)
			{
				Image_item->SetBrushFromTexture(Icon);
				Image_item->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Image_item->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		else
		{
			Image_item->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Text_Count)
	{
		if (bHasItem && CachedSlot.Count > 1)
		{
			Text_Count->SetText(FText::AsNumber(CachedSlot.Count));
			Text_Count->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_Count->SetText(FText::GetEmpty());
			Text_Count->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Text_Name)
	{
		if (bHasItem && CachedSlot.ItemDef)
		{
			const FString Id = CachedSlot.ItemDef->DisplayName.ToString();
			FText NameText = FText::FromString(Id);

			if (UWorld* World = GetWorld())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					if (const UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
					{
						NameText = Loc->GetText(Id);
					}
				}
			}

			Text_Name->SetText(NameText);
			Text_Name->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_Name->SetText(FText::GetEmpty());
			Text_Name->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
