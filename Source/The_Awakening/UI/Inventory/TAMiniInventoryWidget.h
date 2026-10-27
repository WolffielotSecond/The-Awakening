#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/TAItemTypes.h"
#include "TAMiniInventoryWidget.generated.h"

class UTAInventoryComponent;
class UHorizontalBox;
class UTextBlock;
class UImage;

/**
 * 角色旁边的迷你背包：拾取后显示一会儿，按背包同样的口袋布局画出所有格子。
 * - 空格：空心圆角方块；满叠或不可堆叠：实心白；未满的堆叠：按 Count / MaxStack 在灰与白之间插值。
 * - 拾取成功：本次改动的格子闪烁两次。背包已满：上方显示“背包已满”，不闪烁。
 * - 显示 DisplayDuration 秒游戏时间（时停时不走），最后 FadeOutDuration 秒渐变为透明；再次拾取重新计时并恢复不透明。
 * 外观参数都可在 WBP 里调；WBP 中有 HorizontalBox_Clothing / Text_Full 时用它们，没有 WBP 时用 C++ 临时布局。
 */
UCLASS()
class THE_AWAKENING_API UTAMiniInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 拾取后调用。ChangedSlots：TryAddItem 带出的扁平格子下标；bInventoryFull：有物品没放下 */
	void ShowPickupResult(UTAInventoryComponent* InInventory, const TArray<int32>& ChangedSlots, bool bInventoryFull);

	UFUNCTION(BlueprintCallable, Category = "MiniInventory")
	void HideNow();

	UFUNCTION(BlueprintPure, Category = "MiniInventory")
	bool IsShowing() const { return TimeLeft > 0.f; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 【临时】没有 WBP 时用 C++ 搭最简布局 */
	void BuildFallbackLayout();
	void RebuildCellsIfNeeded();
	void RefreshCellVisuals();
	void UpdateScreenPosition();
	void BindInventory(UTAInventoryComponent* InInventory);
	/** Alpha 同时乘到填充色和描边色上：圆角框的描边不受 RenderOpacity 影响，渐隐、闪烁都要直接改颜色 */
	FSlateBrush MakeCellBrush(const FTAInventorySlot& SlotData, float Alpha) const;

	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void RefreshLocalizedText();

protected:
	/** 每件衣服一组，组内每个口袋一个 Columns x Rows 网格 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Clothing;

	/** “背包已满”，显示在格子上方 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Full;

	UPROPERTY(EditAnywhere, Category = "MiniInventory")
	FString FullTextId = TEXT("Inventory_Full");

	/** 显示时长（游戏时间，秒） */
	UPROPERTY(EditAnywhere, Category = "MiniInventory", meta = (ClampMin = "0.1", Units = "s"))
	float DisplayDuration = 4.f;

	/** 消失前渐变为透明的时长（游戏时间，秒），包含在 DisplayDuration 之内 */
	UPROPERTY(EditAnywhere, Category = "MiniInventory", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Blink", meta = (ClampMin = "1"))
	int32 BlinkCount = 2;

	/** 一次闪烁（暗 + 亮）的时长；显示后立刻开始快速闪烁 */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Blink", meta = (ClampMin = "0.05", Units = "s"))
	float BlinkPeriod = 0.16f;

	/** 闪烁暗下去时的不透明度 */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Blink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlinkDimOpacity = 0.1f;

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style", meta = (ClampMin = "2"))
	float CellSize = 14.f;

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style", meta = (ClampMin = "0"))
	float CellSpacing = 2.f;

	/** 口袋之间、衣服之间的间距 */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style", meta = (ClampMin = "0"))
	float PocketSpacing = 6.f;

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style", meta = (ClampMin = "0"))
	float CornerRadius = 3.f;

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style", meta = (ClampMin = "0"))
	float OutlineWidth = 1.5f;

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style")
	FLinearColor OutlineColor = FLinearColor::White;

	/** 满叠 / 不可堆叠 */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style")
	FLinearColor FullColor = FLinearColor::White;

	/** 未满堆叠的最浅一档（数量接近 0 时），数量越多越接近 FullColor */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Style")
	FLinearColor PartialMinColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f);

	/** 跟随的世界位置 = 角色位置 + WorldOffset，再在屏幕上偏移 ScreenOffset */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Position")
	FVector WorldOffset = FVector(0.f, 0.f, 100.f);

	UPROPERTY(EditAnywhere, Category = "MiniInventory|Position")
	FVector2D ScreenOffset = FVector2D(60.f, 0.f);

	/** 控件相对跟随点的对齐（0,1 = 左下角对齐跟随点） */
	UPROPERTY(EditAnywhere, Category = "MiniInventory|Position")
	FVector2D ScreenAlignment = FVector2D(0.f, 1.f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Cells;

	UPROPERTY(Transient)
	TObjectPtr<UTAInventoryComponent> Inventory;

	/** 当前格子对应的布局（衣服定义 + 每个口袋的格子数），变了才重建 */
	TArray<FString> BuiltLayoutKey;
	TSet<int32> BlinkSlots;
	/** 当前整体不透明度（渐隐）和闪烁是否处于暗的半段 */
	float FadeAlpha = 1.f;
	bool bBlinkDim = false;
	float TimeLeft = 0.f;
	float BlinkElapsed = 0.f;
	bool bShowFull = false;
};
