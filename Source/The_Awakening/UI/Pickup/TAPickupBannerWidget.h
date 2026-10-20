#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAPickupBannerWidget.generated.h"

class UImage;
class UTextBlock;
class UHorizontalBox;
class UInputAction;
class UTAItemDefinition;
class UTADocumentDefinition;
class UTAActionPromptWidget;
class ATAPreviewStage;

/** 一条拾取提示：物品或文本（二选一） */
USTRUCT()
struct THE_AWAKENING_API FTAPickupBannerEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTAItemDefinition> Item = nullptr;

	UPROPERTY()
	TObjectPtr<UTADocumentDefinition> Document = nullptr;

	bool IsValid() const { return Item || Document; }
};

/**
 * 拾取后屏幕右侧的提示（WBP_PickupBanner 的父类）。策划文档：
 * - 从屏幕右侧外滑入（0.5 秒）→ 停留 → 滑出（0.5 秒），共 5 秒；真实时间计时，不受扫描时停影响。
 * - 上：顺时针旋转的 3D 模型；中：名称；下：“按 [背包键] 查看详细”。
 * - 文本：先显示标题 1.85 秒，淡出 0.15 秒换成正文前 20 个字 + “...”（跳过图片标记），淡入 0.15 秒，再停 1.85 秒。
 * - 显示期间又拾取：当前这条立刻滑出，下一条再滑入（排队）。
 * - 显示期间按背包键：由角色取走当前条目做快速查看（TakeQuickViewEntry），提示立即隐藏。
 *
 * 控件命名：
 *   Panel_Banner（任意 UWidget）：整个提示，滑入滑出作用在它上面（放在根 Canvas 右侧）
 *   Image_Model（UImage）：3D 模型画面
 *   Text_Name（UTextBlock）：名称 / 标题 / 正文预览
 *   HorizontalBox_Hint（UHorizontalBox）：底部提示，C++ 生成“[背包键] 查看详细”
 * 不接收输入，不申请时停。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAPickupBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 背包键：提示里的按键图标用它 */
	void SetInventoryAction(UInputAction* InAction);

	void ShowItem(UTAItemDefinition* Item);
	void ShowDocument(UTADocumentDefinition* Document);

	/** 正在显示（未开始滑出）的条目；取走后提示立即隐藏、清空队列。没有时返回空条目 */
	FTAPickupBannerEntry TakeQuickViewEntry();

	UFUNCTION(BlueprintPure, Category = "Pickup Banner")
	bool IsShowing() const { return Phase != EPhase::Idle; }

	/** 文本正文预览：去掉 <...> 标记、合并空白后取前 MaxChars 个字，超出时加 “...” */
	static FString MakeBodyPreview(const FString& Body, int32 MaxChars);

	// ==================== 参数（WBP 里调） ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Timing", meta = (ClampMin = "0", Units = "s"))
	float SlideInDuration = 0.5f;

	/** 物品停留时间 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Timing", meta = (ClampMin = "0", Units = "s"))
	float ItemHoldDuration = 4.f;

	/** 文本：标题、正文各自停留的时间 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Timing", meta = (ClampMin = "0", Units = "s"))
	float DocumentTextHoldDuration = 1.85f;

	/** 文本：标题与正文切换的淡出 + 淡入总时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Timing", meta = (ClampMin = "0", Units = "s"))
	float DocumentTextSwapDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Timing", meta = (ClampMin = "0", Units = "s"))
	float SlideOutDuration = 0.5f;

	/** 模型旋转速度（度 / 秒，从上往下看顺时针） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Model")
	float ModelRotationSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Model")
	FIntPoint ModelRenderSize = FIntPoint(512, 512);

	/** 正文预览字数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Text", meta = (ClampMin = "1"))
	int32 BodyPreviewChars = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner|Text")
	FString ViewDetailTextId = TEXT("UI_Pickup_ViewDetail");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup Banner")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	/** 预览台槽位 0 留给拾取提示 */
	static constexpr int32 PreviewStageSlot = 0;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Banner;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Model;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HorizontalBox_Hint;

private:
	enum class EPhase : uint8 { Idle, SlideIn, HoldTitle, SwapText, HoldBody, SlideOut };

	void Enqueue(const FTAPickupBannerEntry& Entry);
	void StartNext();
	void BeginSlideOut();
	void ApplySlide(float Visible);
	void HideImmediately();
	FText Localize(const FString& TextId) const;
	float GetHoldDuration() const;

	UFUNCTION()
	void HandleLanguageChanged();

	UPROPERTY(Transient)
	TArray<FTAPickupBannerEntry> Queue;

	UPROPERTY(Transient)
	FTAPickupBannerEntry Current;

	UPROPERTY(Transient)
	TObjectPtr<ATAPreviewStage> PreviewStage;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InventoryAction;

	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.f;
	/** 0 完全在屏幕外，1 完全滑入；滑出从当前位置开始 */
	float SlideVisible = 0.f;
	float SlideOutFrom = 1.f;
	bool bShowingBody = false;
};
