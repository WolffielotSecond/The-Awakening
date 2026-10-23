#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/TAInputRouter.h"
#include "Core/TATimeSubsystem.h"
#include "Health/TADamageTypes.h"
#include "TARespawnSubsystem.generated.h"

class AThe_AwakeningPlayerController;
class ATACheckpoint;
class AThe_AwakeningCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTAWorldResetSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTACheckpointChangedSignature, ATACheckpoint*, Checkpoint);

/** 复活流程阶段。 */
UENUM(BlueprintType)
enum class ETARespawnPhase : uint8
{
	/** 存活，没有复活流程。 */
	None,
	/** 死亡中（DeathDelay）：死亡动画、空中死亡落地。 */
	Dying,
	/** 黑屏（FadeOutTime）：镜头淡出到全黑。 */
	FadingOut,
	/** 淡入（FadeInTime）：复位已完成，镜头从全黑淡入，结束时交还控制。 */
	FadingIn
};

/**
 * 复活流程的驱动者：复活点登记、死亡输入请求的保管、5 个阶段的推进。
 * 存活 → 死亡中 → 黑屏 → 复位（黑屏中同一帧：传送、回血、清临时修改器、OnWorldReset）→ 淡入 → 存活。
 * Pawn 不销毁、不走 RestartPlayer，背包和组件状态都保留。所有阶段计时用游戏时间（UTATimeSubsystem）。
 */
UCLASS()
class THE_AWAKENING_API UTARespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 死亡输入请求优先级：压过暂停页（500）、设置（600）和子菜单（610）。 */
	static constexpr int32 DeathInputPriority = 900;

	static UTARespawnSubsystem* Get(const UObject* WorldContext);

	// -------------------- 复活点 --------------------
	/** 复活点 BeginPlay / EndPlay 时自动调用。 */
	void RegisterCheckpoint(ATACheckpoint* Checkpoint);
	void UnregisterCheckpoint(ATACheckpoint* Checkpoint);
	/** 无条件替换为当前复活点（最后交互的生效）。 */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void ActivateCheckpoint(ATACheckpoint* Checkpoint);
	UFUNCTION(BlueprintPure, Category = "Respawn")
	ATACheckpoint* GetActiveCheckpoint() const { return ActiveCheckpoint.Get(); }
	UFUNCTION(BlueprintPure, Category = "Respawn")
	ATACheckpoint* FindCheckpoint(FName CheckpointId) const;

	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FTACheckpointChangedSignature OnCheckpointChanged;

	// -------------------- 死亡与复活 --------------------
	/**
	 * 申请死亡输入请求：Allowed 为空、GameOnly、不显示光标。请求的 Owner 是本子系统，
	 * 句柄要么交给 BeginRespawn 保管，要么由调用方 ReleaseDeathInput 释放。返回 0 表示失败。
	 */
	FTAInputRouter::FHandle AcquireDeathInput(AThe_AwakeningPlayerController* PC);
	void ReleaseDeathInput(AThe_AwakeningPlayerController* PC, FTAInputRouter::FHandle Handle);

	/** 由生命组件在死亡清理完成后调用。已在复活中则返回 false 并不接管句柄。 */
	bool BeginRespawn(AThe_AwakeningPlayerController* PC, FTAInputRouter::FHandle DeathInputHandle);

	/** Default implementations for the character presentation events; also callable from a Blueprint override's fallback branch. */
	UFUNCTION(BlueprintCallable, Category = "Respawn|Presentation")
	void RunDefaultDeathPresentation(AThe_AwakeningCharacter* Character, int32 RunId);
	UFUNCTION(BlueprintCallable, Category = "Respawn|Presentation")
	void RunDefaultBlackoutPresentation(AThe_AwakeningCharacter* Character, int32 RunId);
	/** Complete only the matching run and phase. Stale, duplicate and other-pawn callbacks are ignored. */
	UFUNCTION(BlueprintCallable, Category = "Respawn|Presentation")
	void CompleteDeathPresentation(AThe_AwakeningCharacter* Character, int32 RunId);
	UFUNCTION(BlueprintCallable, Category = "Respawn|Presentation")
	void CompleteBlackoutPresentation(AThe_AwakeningCharacter* Character, int32 RunId);

	/** 敌人 AI 用来停止攻击。 */
	UFUNCTION(BlueprintPure, Category = "Respawn")
	bool IsRespawning() const { return Phase != ETARespawnPhase::None; }
	UFUNCTION(BlueprintPure, Category = "Respawn")
	ETARespawnPhase GetRespawnPhase() const { return Phase; }

	FTAInputRouter::FHandle GetDeathInputHandle() const { return DeathInputHandle; }

	/** 复位步骤广播一次。敌人、可重置机关订阅它；已开的门、已拾的物品、剧情旗标不重置（没有死亡惩罚）。 */
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FTAWorldResetSignature OnWorldReset;

protected:
	virtual void Deinitialize() override;

	void EnterFadeOut();
	/** 黑屏中同一帧完成：传送、镜头复位、回血、清临时修改器与 ClearOnRespawn 效果、广播 OnWorldReset，然后开始淡入。 */
	void ResetAndFadeIn();
	/** 复活收尾：移除死亡状态、恢复扫描、广播 OnRespawned、加复活无敌、最后交还输入。 */
	void CompleteRespawn();

	/** 当前复活点；没有时退回到 PlayerStart（并打 Warning）；都没有则原地。 */
	bool ResolveRespawnTransform(AThe_AwakeningPlayerController* PC, FTransform& OutTransform) const;

private:
	void StartCameraFade(AThe_AwakeningPlayerController* PC, float From, float To, float Duration, bool bHold) const;
	void ClearPhaseTimer();
	void ClearPresentationWatchdog();
	void ArmPresentationWatchdog();
	void HandlePresentationTimeout();
	bool MatchesPresentation(AThe_AwakeningCharacter* Character, int32 RunId, ETARespawnPhase ExpectedPhase) const;
	UFUNCTION() void PresentationCharacterDestroyed(AActor* Actor);
	void HandleRespawnProtectionExpired();

	TArray<TWeakObjectPtr<ATACheckpoint>> Checkpoints;
	TWeakObjectPtr<ATACheckpoint> ActiveCheckpoint;

	ETARespawnPhase Phase = ETARespawnPhase::None;
	TWeakObjectPtr<AThe_AwakeningPlayerController> RespawningController;
	FTAInputRouter::FHandle DeathInputHandle = 0;
	FTAGameplayTimerHandle PhaseTimer;
	FTAGameplayTimerHandle PresentationWatchdog;
	TWeakObjectPtr<AThe_AwakeningCharacter> PresentationCharacter;
	int32 PresentationRunId = 0;
	FGameplayTag DeathDamageSource;
	ETADamageType DeathDamageType = ETADamageType::Normal;
	bool bDefaultPresentationStarted = false;

	FTAHealthProtectionHandle RespawnProtection;
	TWeakObjectPtr<class UTAHealthComponent> ProtectedHealth;
	FTAGameplayTimerHandle ProtectionTimer;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FTADeathCleanupTest;
	friend class FTADeathInputTest;
	friend class FTARespawnFlowTest;
#endif
};
