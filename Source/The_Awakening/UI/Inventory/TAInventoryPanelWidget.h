#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAInventoryPanelWidget.generated.h"

class UTAInventoryComponent;
class UTAInventorySlotWidget;
class UTAClothingPanelWidget;
class UBorder;
class UTextBlock;
class UWidgetSwitcher;
class UButton;
class UNamedSlot;

UCLASS()
class THE_AWAKENING_API UTAInventoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "InventoryUI")
	void Init(UTAInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "InventoryUI")
	void RefreshAll();

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	void EnsureDynamicChildren();

	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void RefreshLocalizedChrome();

	UFUNCTION()
	void HandleLanguageChanged();

	UFUNCTION()
	void OnClickInventoryTab();

	UFUNCTION()
	void OnClickSkillsTab();

protected:
	UPROPERTY()
	TObjectPtr<UTAInventoryComponent> Inventory;

	// ----- 与 WBP_InventoryPanel 命名一致 -----
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Dim;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Money;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Inventory;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Skills;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_OuterEquip;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_Story0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_Story1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_Story2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_Story3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_OuterClothing;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> NamedSlot_InnerClothing;

	// ----- 动态创建 -----
	UPROPERTY()
	TObjectPtr<UTAInventorySlotWidget> OuterEquipSlot;

	UPROPERTY()
	TArray<TObjectPtr<UTAInventorySlotWidget>> StorySlots;

	UPROPERTY()
	TObjectPtr<UTAClothingPanelWidget> OuterClothingPanel;

	UPROPERTY()
	TObjectPtr<UTAClothingPanelWidget> InnerClothingPanel;

	UPROPERTY(EditAnywhere, Category = "InventoryUI")
	TSubclassOf<UTAClothingPanelWidget> ClothingPanelClass;

	UPROPERTY(EditAnywhere, Category = "InventoryUI")
	TSubclassOf<UTAInventorySlotWidget> SlotWidgetClass;

	/** 顶栏「背包」按钮上的文字 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Inventory;

	/** 顶栏「技能」按钮上的文字 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Skills;

	/** 技能页占位文字（暂不可用） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkillsPlaceholder;
};
