#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "Health/TAHealthComponent.h"
#include "TAClothingSwapWidget.generated.h"

class ATAClothingRack;
class UTAInventoryComponent;
class UTAClothingPanelWidget;
class UTAInventorySlotWidget;
class UTAItemDefinition;
class UNamedSlot;
class UTextBlock;
class UInputAction;

/**
	 * 衣架换装界面。左边：衣架上的外套格、纯外套图和口袋；右边：身上的外套格、完整穿着图、内衬和外套口袋。
 * - 外套格之间拖拽：穿上 / 换下 / 互换外套，口袋里的物品跟着衣服走。
 * - 口袋格之间拖拽：衣架口袋 ↔ 身上口袋移动、合并、交换物品（剧情物品不能放进衣架）。
 * 打开期间时停（与背包相同）并无敌；ESC（共享 UI 返回键）关闭。
 * 固定左右半屏，衣架口袋在图左，穿着口袋在图右；复用背包的自适应大 Grid 框。
 */
UCLASS()
class THE_AWAKENING_API UTAClothingSwapWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	/** 创建并显示换装界面。WidgetClass 为空时用 /Game/UI/Inventory/WBP_ClothingSwap，再没有时用 C++ 类 */
	static UTAClothingSwapWidget* Open(APlayerController* PC, ATAClothingRack* InRack, UTAInventoryComponent* InInventory,
		TSubclassOf<UTAClothingSwapWidget> WidgetClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "ClothingSwap")
	void Init(ATAClothingRack* InRack, UTAInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "ClothingSwap")
	void Close() { RemoveFromParent(); }

	UFUNCTION(BlueprintCallable, Category = "ClothingSwap")
	void Refresh();

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override { Close(); return true; }
	bool AllowsPlayerInput(ETAInputCapability Capability) const;

	virtual void RemoveFromParent() override;

	/** 放下处理（鼠标拖拽和手柄选取共用）。返回是否有改动 */
	bool HandleSlotDrop(UTAInventorySlotWidget* Source, UTAInventorySlotWidget* Target);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

	/** 格子属于哪一边 */
	enum class ESlotOwner : uint8 { None, RackCoat, PlayerCoat, RackPocket, PlayerPocket };
	ESlotOwner ResolveSlot(const UTAInventorySlotWidget* InSlot, int32& OutPocket, int32& OutSlotInPocket) const;

	void EnsureChildren();
	void ConfigureSlot(UTAInventorySlotWidget* InSlot);
	void ConfigurePanelSlots(UTAClothingPanelWidget* Panel);
	/** Fixed canvas anchors keep coat slots in place even when an entire side is empty. */
	void BuildSwapLayout();
	void AcquireGuards();
	void ReleaseGuards();
	void SwapCoats();
	void ToggleGamepadSelect();
	void CancelGamepadSelect();
	void SimulateLeftMouseClick();

	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandleRackChanged();

	UFUNCTION()
	void HandleSlotHovered(UTAInventorySlotWidget* HoveredSlot, UTAItemDefinition* ItemDef);

	UFUNCTION()
	void HandleSlotUnhovered(UTAInventorySlotWidget* HoveredSlot);

	UFUNCTION()
	void RefreshLocalizedText();

protected:
	// ----- 与 WBP_ClothingSwap 命名一致（都可选） -----
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> NamedSlot_RackCoat;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> NamedSlot_RackClothing;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> NamedSlot_PlayerCoat;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> NamedSlot_InnerClothing;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UNamedSlot> NamedSlot_OuterClothing;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RackTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PlayerTitle;

	/** 为空时用 /Game/UI/Inventory/WBP_ClothingPanel */
	UPROPERTY(EditAnywhere, Category = "ClothingSwap")
	TSubclassOf<UTAClothingPanelWidget> ClothingPanelClass;

	/** 为空时用 /Game/UI/Inventory/WBP_InventorySlot */
	UPROPERTY(EditAnywhere, Category = "ClothingSwap")
	TSubclassOf<UTAInventorySlotWidget> SlotWidgetClass;

	/** 为空时用背包同一个 IA_Inventory_Confirm（手柄确认 = 模拟左键点击） */
	UPROPERTY(EditAnywhere, Category = "ClothingSwap|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	/** 为空时用背包同一个 IA_Inventory_Drag（手柄：第一次按选中，第二次按放到光标所在格子） */
	UPROPERTY(EditAnywhere, Category = "ClothingSwap|Input")
	TObjectPtr<UInputAction> GamepadDragModeAction;

	UPROPERTY(EditAnywhere, Category = "ClothingSwap")
	FString RackTitleTextId = TEXT("UI_ClothingRack_Rack");

	UPROPERTY(EditAnywhere, Category = "ClothingSwap")
	FString PlayerTitleTextId = TEXT("UI_ClothingRack_Worn");

	// ----- 动态创建 -----
	UPROPERTY(Transient)
	TObjectPtr<UTAInventorySlotWidget> RackCoatSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTAInventorySlotWidget> PlayerCoatSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTAClothingPanelWidget> RackClothingPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTAClothingPanelWidget> InnerClothingPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTAInventoryComponent> Inventory;

	TWeakObjectPtr<ATAClothingRack> Rack;
	TWeakObjectPtr<UTAInventorySlotWidget> HoveredSlot;
	TWeakObjectPtr<UTAInventorySlotWidget> GamepadSourceSlot;

private:
	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
	/** 打开期间时停 + 无敌，与输入请求一起申请、一起释放 */
	bool bFreezeRequested = false;
	TWeakObjectPtr<UTAHealthComponent> ProtectedHealth;
	FTAHealthProtectionHandle InvulnerableHandle;
};
