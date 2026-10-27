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
class UTAClothingDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTAInventorySlotHovered, class UTAInventorySlotWidget*, Slot, UTAItemDefinition*, ItemDef);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAInventorySlotUnhovered, class UTAInventorySlotWidget*, Slot);

UCLASS()
class THE_AWAKENING_API UTAInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual bool NativeIsInteractable() const override { return GetIsEnabled() && !IsEmpty() && AllowsPlayerDrag(); }
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

	/** 外套格：显示衣服图标（不是物品）。传 nullptr 清空 */
	void SetClothingDisplay(UTAClothingDefinition* InClothing);
	UTAClothingDefinition* GetClothingDisplay() const { return DisplayClothing; }

	/** false：只显示，不能拖出也不接受拖入（例如背包里的外套格） */
	void SetDragEnabled(bool bEnabled) { bDragEnabled = bEnabled; }

	/** 非背包界面（衣架换装）接管格子：拖拽权限由它判断，放下时交给它处理，不走背包的 MoveItemBetweenSlots */
	void SetExternalDragHandlers(TFunction<bool()> InPermission, TFunction<bool(UTAInventorySlotWidget* Source, UTAInventorySlotWidget* Target)> InDropHandler);
	void SetDraggingVisual(bool bDragging);
	const FGeometry& GetIconGeometry() const;
	UWidget* GetIconWidget() const;

	UFUNCTION(BlueprintCallable, Category = "InventorySlot")
	int32 GetFlatIndex() const { return FlatIndex; }

	/** 左侧剧情格：可悬停、可选中，但不能拖拽 */
	UFUNCTION(BlueprintPure, Category = "InventorySlot")
	bool IsStorySlot() const;

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

	UPROPERTY()
	TObjectPtr<UTAClothingDefinition> DisplayClothing;

	bool bDragEnabled = true;
	TFunction<bool()> ExternalDragPermission;
	TFunction<bool(UTAInventorySlotWidget*, UTAInventorySlotWidget*)> ExternalDropHandler;

	bool bIsDragging = false;

	UPROPERTY()
	int32 FlatIndex = INDEX_NONE;
};
