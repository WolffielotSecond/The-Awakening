#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/TAItemTypes.h"
#include "TAInventorySlotWidget.generated.h"

class UImage;
class UTextBlock;
class UBorder;
class USizeBox;
class UTAItemDefinition;
class UTAInventoryComponent;
class UDragDropOperation;
class UTAInventoryPanelWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTAInventorySlotHovered, class UTAInventorySlotWidget*, Slot, UTAItemDefinition*, ItemDef);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAInventorySlotUnhovered, class UTAInventorySlotWidget*, Slot);

UCLASS()
class THE_AWAKENING_API UTAInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION(BlueprintCallable, Category = "InventorySlot")
	void SetSlotData(const FTAInventorySlot& SlotData, int32 FlatIndex);

	UFUNCTION(BlueprintCallable, Category = "InventorySlot")
	void SetEmpty();

	void SetInventoryComponent(UTAInventoryComponent* InInventory) { Inventory = InInventory; }
	void SetInputOwner(UTAInventoryPanelWidget* InOwner);
	void SetDraggingVisual(bool bDragging);

	UFUNCTION(BlueprintCallable, Category = "InventorySlot")
	int32 GetFlatIndex() const { return FlatIndex; }

	UFUNCTION(BlueprintCallable, Category = "InventorySlot")
	bool IsEmpty() const;

	UFUNCTION(BlueprintCallable, Category = "InventorySlot")
	UTAItemDefinition* GetItemDef() const;

	const FTAInventorySlot& GetSlotData() const { return CachedSlot; }

	UPROPERTY(BlueprintAssignable, Category = "InventorySlot|Hover")
	FOnTAInventorySlotHovered OnInventorySlotHovered;

	UPROPERTY(BlueprintAssignable, Category = "InventorySlot|Hover")
	FOnTAInventorySlotUnhovered OnInventorySlotUnhovered;

protected:
	void RefreshVisuals();
	bool AllowsPlayerDrag() const;
	TWeakObjectPtr<UTAInventoryPanelWidget> InputOwner;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_Root; // 若根 Size Box 没改名，可删掉此绑定

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_item;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Count;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY()
	FTAInventorySlot CachedSlot;

	UPROPERTY()
	TObjectPtr<UTAInventoryComponent> Inventory;

	bool bIsDragging = false;

	UPROPERTY()
	int32 FlatIndex = INDEX_NONE;
};
