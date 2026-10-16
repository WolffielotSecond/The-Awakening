#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TATimeSubsystem.generated.h"

DECLARE_DYNAMIC_DELEGATE(FTAGameplayTimerDynamicDelegate);

/** 游戏时间计时器句柄。0 表示无效。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAGameplayTimerHandle
{
	GENERATED_BODY()

	uint64 Id = 0;
	bool IsValid() const { return Id != 0; }
	void Invalidate() { Id = 0; }
	bool operator==(const FTAGameplayTimerHandle& Other) const { return Id == Other.Id; }
};

/**
 * 统一时停入口。所有需要停时间的功能都只调这里，按来源申请、按来源释放：
 * - 冻结（Freeze）：只冻结挂了 TAFreezeComponent 的实体，世界时间、特效、UI 照常运行。扫描渐变、小游戏完全冻结。
 *   具体的实体冻结仍由 UTAFreezeSubsystem 执行，行为与原来一致。
 * - 全局暂停（Pause）：引擎 SetGamePaused。任意来源持有即暂停，最后一个来源释放才恢复，互不误解除。
 *
 * 游戏时间（GameplayTime）：每帧累加 世界 DeltaTime × (1 − 冻结强度)，全局暂停时不走。
 * 玩法计时器（修改器持续时间、复活流程、以后的 Buff / 冷却等）一律用 SetGameplayTimer。
 * 例外：发起时停的功能自身的计时（扫描淡入淡出、小游戏倒计时与结算）和菜单 / 弹窗的计时使用真实或世界时间，
 * 否则会被自己发起的时停卡住。
 */
UCLASS()
class THE_AWAKENING_API UTATimeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTATimeSubsystem* Get(const UObject* WorldContext);

	// -------------------- 冻结 --------------------
	/** 建立或更新冻结请求，Strength 0～1（0 也保留请求）。只有 ReleaseFreeze 结束请求。多个来源取最大强度。 */
	void RequestFreeze(UObject* Source, float Strength = 1.f);
	void ReleaseFreeze(UObject* Source);
	UFUNCTION(BlueprintPure, Category = "Time")
	float GetFreezeStrength() const;

	// -------------------- 全局暂停 --------------------
	void RequestPause(UObject* Source);
	void ReleasePause(UObject* Source);
	UFUNCTION(BlueprintPure, Category = "Time")
	bool HasPauseRequest() const;

	// -------------------- 游戏时间 --------------------
	/** 本 World 开始以来累计的游戏时间（秒）。 */
	UFUNCTION(BlueprintPure, Category = "Time")
	double GetGameplayTime() const { return GameplayTime; }
	/** 当前游戏时间倍率：世界时间倍率 × (1 − 冻结强度)，暂停时为 0。 */
	UFUNCTION(BlueprintPure, Category = "Time")
	float GetGameplayTimeScale() const;

	/** 按游戏时间计时。Duration <= 0 时下一次游戏时间推进即触发。循环计时器 Duration 必须 > 0。 */
	FTAGameplayTimerHandle SetGameplayTimer(FTimerDelegate Delegate, float Duration, bool bLoop = false);
	UFUNCTION(BlueprintCallable, Category = "Time", meta = (DisplayName = "Set Gameplay Timer by Event"))
	FTAGameplayTimerHandle SetGameplayTimerByEvent(FTAGameplayTimerDynamicDelegate Event, float Duration, bool bLoop = false);
	UFUNCTION(BlueprintCallable, Category = "Time")
	void ClearGameplayTimer(UPARAM(ref) FTAGameplayTimerHandle& Handle);
	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsGameplayTimerActive(FTAGameplayTimerHandle Handle) const;
	/** 剩余游戏时间（秒），无效句柄返回 -1。 */
	UFUNCTION(BlueprintPure, Category = "Time")
	float GetGameplayTimerRemaining(FTAGameplayTimerHandle Handle) const;

	/** 推进游戏时间并触发到期计时器。Tick 内部调用；测试可直接调用以模拟帧。 */
	void AdvanceGameplayTime(float WorldDeltaSeconds);

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// 暂停期间也要 Tick，用于清理已销毁的暂停来源；游戏时间在暂停时不累加。
	virtual bool IsTickableWhenPaused() const override { return true; }

protected:
	virtual void Deinitialize() override;

private:
	struct FTimerEntry
	{
		uint64 Id = 0;
		double FireTime = 0.0;
		float Interval = 0.f;
		bool bLoop = false;
		FTimerDelegate Delegate;
	};

	void PrunePauseSources();
	void ApplyPauseState();

	TSet<TWeakObjectPtr<UObject>> PauseSources;
	bool bAppliedPause = false;

	double GameplayTime = 0.0;
	TArray<FTimerEntry> Timers;
	uint64 NextTimerId = 0;
};
