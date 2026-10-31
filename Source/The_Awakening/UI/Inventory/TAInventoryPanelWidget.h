#pragma once

#include "CoreMinimal.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "Blueprint/UserWidget.h"
#include "TAInventoryPanelWidget.generated.h"

class UTAInventoryComponent;
class UTAInventoryTabBar;
class UTexture2D;
class UTAInventorySlotWidget;
class UTAClothingPanelWidget;
class UBorder;
class UTextBlock;
class UWidgetSwitcher;
class UButton;
class UNamedSlot;
class UImage;
class UHorizontalBox;
class USizeBox;
class UInputAction;
class UTAActionPromptWidget;
class UTAItemDefinition;
class UTAScanInfoWidget;
struct FInputActionValue;

UCLASS()
class THE_AWAKENING_API UTAInventoryPanelWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override { RemoveFromParent(); return true; }
	bool AllowsPlayerInput(ETAInputCapability Capability) const;
	virtual void RemoveFromParent() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "InventoryUI")
	void Init(UTAInventoryComponent* InInventory, UInputAction* InCloseAction = nullptr);
	UPROPERTY(Transient) TObjectPtr<UInputAction> CloseAction;

	UFUNCTION(BlueprintCallable, Category = "InventoryUI")
	void RefreshAll();
	UWidget* GetDiaryIconWidget() const;

	/** Refresh shortcut icons and bottom-row prompts using the currently active input device. */
	UFUNCTION(BlueprintCallable, Category = "InventoryUI|Input")
	void RefreshInputPrompts();

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
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

	UFUNCTION()
	void SelectInventoryPage(int32 Page);

	void EnsureInventoryInputActions();
	void RefreshInputIcons();
	void RefreshHeaderStats(bool bForce = false);
	UFUNCTION()
	void HandleInputPromptsChanged();
	void ToggleGamepadDragMode(const FInputActionValue& Value);
	void HandleConfirmPressed();
	void BeginGamepadDragMode(UTAInventorySlotWidget* SourceSlot);
	void CancelGamepadDragMode();
	void CommitGamepadDragMode();
	void UpdateGamepadDragVisualPosition();
	void SimulateLeftMouseClick();
	UFUNCTION()
	void OnActionPromptClicked(UTAActionPromptWidget* Prompt);
	void ChangePage(int32 Direction);
	void BuildActionPromptBar();
	void BindSlotHoverEvents(UTAInventorySlotWidget* InventorySlot);
	void ShowItemInfo(UTAInventorySlotWidget* InventorySlot, UTAItemDefinition* ItemDef);
	void HideItemInfo(UTAInventorySlotWidget* InventorySlot);
	void UpdateItemInfoWidgetPosition();
	UFUNCTION()
	void HandleInventorySlotHovered(UTAInventorySlotWidget* HoveredSlot, UTAItemDefinition* ItemDef);
	/** 在光标处打开该格子物品的交互菜单；没有可用选项时不打开 */
	void OpenItemActionMenu(UTAInventorySlotWidget* InventorySlot);
	void HandleItemAction(FName ActionId, TWeakObjectPtr<UTAInventorySlotWidget> SourceSlot);
	/** 在背包里显示一条提示（WBP 中有 Text_Message 时用它，没有时退回屏幕调试文字） */
	void ShowMessage(const FString& TextId);
	UFUNCTION()
	void HandleInventorySlotUnhovered(UTAInventorySlotWidget* HoveredSlot);

protected:
	UPROPERTY()
	TObjectPtr<UTAInventoryComponent> Inventory;

	// ----- 与 WBP_InventoryPanel 命名一致 -----
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Dim;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTAInventoryTabBar> TabBar_Inventory;

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
	TObjectPtr<UTAClothingPanelWidget> InnerClothingPanel;

	UPROPERTY(EditAnywhere, Category = "InventoryUI")
	TSubclassOf<UTAClothingPanelWidget> ClothingPanelClass;

	UPROPERTY(EditAnywhere, Category = "InventoryUI")
	TSubclassOf<UTAInventorySlotWidget> SlotWidgetClass;

	/** Only the worn-coat display slot uses this frame; pocket/story slots keep their own brush. */
	UPROPERTY(EditAnywhere, Category = "InventoryUI|Appearance")
	TObjectPtr<UTexture2D> OuterEquipBackgroundTexture;

	/** 顶栏「背包」按钮上的文字 */
	/** 可选：背包里的提示文字（例如“当前位置无法掉落物品”），显示 MessageDuration 秒后隐藏 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Message;

	UPROPERTY(EditAnywhere, Category = "InventoryUI", meta = (ClampMin = "0.5", Units = "s"))
	float MessageDuration = 2.5f;

	float MessageTimeLeft = 0.f;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_PreviousPageKey;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_NextPageKey;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float PageShortcutIconHeight = 28.0f;

	/** Optional bottom-row action prompt container, matching the dialogue UI pattern. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	/** Actions should be assigned to assets mapped in the controller's shared IMC_UI. */
	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> PreviousPageAction;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> NextPageAction;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	/** Gamepad-only toggle for selecting and placing an inventory item. */
	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> GamepadDragModeAction;

	/** 光标停在物品上时打开它的交互菜单。为空时用 /Game/Input/Actions/UI/IA_ItemActions，按键（键鼠、手柄）都在 IMC_UI 中配置 */
	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TObjectPtr<UInputAction> ItemActionsAction;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString PreviousPagePromptTextId = TEXT("UI_Inventory_PreviousPage");

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString NextPagePromptTextId = TEXT("UI_Inventory_NextPage");

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString ConfirmPromptTextId = TEXT("UI_Inventory_Confirm");

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Input")
	FString GamepadDragModePromptTextId = TEXT("UI_Inventory_DragMode");

	/** Defaults to the same WBP_ScanInfo used by the scanning UI. */
	UPROPERTY(EditAnywhere, Category = "InventoryUI|Item Hover")
	TSubclassOf<UTAScanInfoWidget> ItemInfoWidgetClass;

	UPROPERTY(EditAnywhere, Category = "InventoryUI|Item Hover")
	FVector2D ItemInfoCursorOffset = FVector2D(20.0f, 20.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAActionPromptWidget>> ActionPromptWidgets;

	UPROPERTY(Transient)
	TObjectPtr<class UTAInputIconSubsystem> InputIconSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UTAScanInfoWidget> ItemInfoWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTAInventorySlotWidget> GamepadDragVisualWidget;

	TWeakObjectPtr<UTAInventorySlotWidget> HoveredItemSlot;
	TWeakObjectPtr<UTAInventorySlotWidget> CurrentHoveredInventorySlot;
	TWeakObjectPtr<UTAInventorySlotWidget> GamepadDragSourceSlot;

	bool bGamepadDragModeActive = false;
	bool bWasItemDragActive = false;
private:
	int32 CachedHeaderCells = INDEX_NONE;
	int32 CachedHeaderHealth = INDEX_NONE;
	bool bHeaderStatsInitialized = false;
	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
	/** 背包打开期间时停（策划文档：背包打开时也会时停）。与输入请求一起申请、一起释放。 */
	bool bFreezeRequested = false;
	void ReleaseInput();

};
