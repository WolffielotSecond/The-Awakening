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
class UImage;
class UHorizontalBox;
class UInputAction;
class UInputMappingContext;
class UTAActionPromptWidget;
struct FInputActionValue;

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

	/** Called when the inventory confirm action is pressed; implement item/slot confirmation in the widget Blueprint. */
	UFUNCTION(BlueprintNativeEvent, Category = "InventoryUI|Input")
	void OnInventoryConfirmPressed();
	virtual void OnInventoryConfirmPressed_Implementation();

protected:
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

	void EnsureInventoryInputActions();
	void BindInventoryInputActions();
	void UnbindInventoryInputActions();
	void PushInventoryMappingContext();
	void PopInventoryMappingContext();
	void RefreshInputIcons();
	void HandleInputDeviceChanged();
	void PreviousPage(const FInputActionValue& Value);
	void NextPage(const FInputActionValue& Value);
	void ConfirmPageAction(const FInputActionValue& Value);
	UFUNCTION()
	void OnActionPromptClicked(UTAActionPromptWidget* Prompt);
	void ChangePage(int32 Direction);
	void BuildActionPromptBar();

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

	/** Optional shortcut icons shown beside the inventory page tabs. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_PreviousPageKey;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_NextPageKey;

	/** Optional bottom-row action prompt container, matching the dialogue UI pattern. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	/** Actions should be assigned to the matching actions in the inventory IMC. */
	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> PreviousPageAction;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> NextPageAction;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputMappingContext> InventoryMappingContext;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input", meta = (ClampMin = "0"))
	int32 InventoryMappingPriority = 20;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString PreviousPagePromptTextId = TEXT("UI_Inventory_PreviousPage");

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString NextPagePromptTextId = TEXT("UI_Inventory_NextPage");

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString ConfirmPromptTextId = TEXT("UI_Inventory_Confirm");

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeInventoryMappingContext;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAActionPromptWidget>> ActionPromptWidgets;

	UPROPERTY(Transient)
	TObjectPtr<class UTAInputIconSubsystem> InputIconSubsystem;

	TArray<uint32> InputBindingHandles;
	bool bInventoryMappingPushed = false;
};
