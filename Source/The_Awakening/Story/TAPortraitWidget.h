// Source/The_Awakening/Story/TAPortraitWidget.h
// 单张立绘控件（WBP_Portrait 的 C++ 基类）
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Story/TADialogueTypes.h"
#include "TAPortraitWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 三层立绘（从下到上）：
 *   Image_Base（基础立绘）→ Image_Eyes（眼睛）→ Image_Mouth（嘴巴）
 * 功能：眨眼（随机间隔）、说话动嘴（只有说话者）、位置/缩放补间、淡出销毁。
 * 必须直接放在 UCanvasPanel 下（自身管理 CanvasSlot 位置与尺寸）。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAPortraitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 应用立绘状态（合并后的完整状态；贴图只加载变化的部分，位置变化走补间） */
	void ApplyPortrait(const FTAPortraitEntry& Entry);

	/** 设置是否在说话（只有说话者的嘴巴会动） */
	void SetTalking(bool bInTalking) { bTalking = bInTalking; }

	/** 淡出并在结束后自我移除 */
	void FadeOutAndRemove();

	FString GetCharacterId() const { return Current.CharacterId; }
	bool IsFadingOut() const { return bFadingOut; }

	// ==================== 动画配置 ====================

	/** 眨眼随机间隔（秒） */
	float BlinkMinInterval = 2.5f;
	float BlinkMaxInterval = 6.f;
	/** 闭眼持续时间（秒） */
	float BlinkClosedDuration = 0.12f;
	/** 连眨两次概率 */
	float DoubleBlinkChance = 0.1f;
	/** 说话时嘴巴切换间隔（秒） */
	float MouthToggleInterval = 0.12f;
	/** 位置/缩放补间时长（秒） */
	float MoveDuration = 0.3f;
	/** 淡出时长（秒） */
	float FadeDuration = 0.2f;
	/** 立绘基准高度占视口高度的比例 */
	float BaseHeightRatio = 0.6f;

protected:
	UTexture2D* LoadTexture(const TSoftObjectPtr<UTexture2D>& TextureAsset);

	void UpdateLayerTextures();
	void ScheduleNextBlink();
	void SetEyesClosed(bool bClosed);
	void SetMouthOpen(bool bOpen);

	/** 由父级画布尺寸换算像素目标 */
	void ResolvePixelTargets(const FVector2D& ViewportSize);

	// 三层（WBP_Portrait 中按此命名摆放，重叠布局）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Base;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Eyes;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Mouth;

protected:
	FTAPortraitEntry Current;

	// 已加载贴图缓存
	TObjectPtr<UTexture2D> TexBase;
	TObjectPtr<UTexture2D> TexEyesOpen;
	TObjectPtr<UTexture2D> TexEyesClosed;
	TObjectPtr<UTexture2D> TexMouthOpen;
	TObjectPtr<UTexture2D> TexMouthClosed;

	// 上一次应用的资产引用，避免重复加载。
	TSoftObjectPtr<UTexture2D> LastBaseAsset;
	TSoftObjectPtr<UTexture2D> LastEyesOpenAsset;
	TSoftObjectPtr<UTexture2D> LastEyesClosedAsset;
	TSoftObjectPtr<UTexture2D> LastMouthOpenAsset;
	TSoftObjectPtr<UTexture2D> LastMouthClosedAsset;

	bool bTalking = false;
	bool bMouthOpen = false;
	bool bEyesClosed = false;

	// 眨眼状态
	float BlinkTimer = 0.f;
	float BlinkStateTimer = 0.f;
	bool bBlinking = false;
	bool bSecondBlinkPending = false;

	// 嘴
	float MouthTimer = 0.f;

	// 位置/尺寸补间
	bool bPendingTarget = false;
	bool bHasTarget = false;
	bool bHasInitialPlacement = false;
	FVector2D TargetPositionPx = FVector2D::ZeroVector;
	FVector2D TargetSizePx = FVector2D::ZeroVector;
	FVector2D StartPositionPx = FVector2D::ZeroVector;
	FVector2D StartSizePx = FVector2D::ZeroVector;
	float TweenProgress = 1.f;

	// 淡出
	bool bFadingOut = false;
	float FadeProgress = 0.f;
};
