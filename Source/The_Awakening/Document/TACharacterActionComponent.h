#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/TATimeSubsystem.h"
#include "Core/TAInputRouter.h"
#include "Health/TADamageTypes.h"
#include "TACharacterActionComponent.generated.h"

class AThe_AwakeningPlayerController;

/** 一种角色动作的配置：动作期间限制哪些输入、是否无敌、没有动画时多久完成。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTACharacterActionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (Categories = "Anim"))
	FGameplayTag Tag;

	/** 没有动画蓝图认领时，经过这么久（游戏时间，秒）自动完成 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.0"))
	float FallbackDuration = 0.6f;

	/** 动画蓝图认领后迟迟不调用 NotifyActionFinished 时的保底完成时间（游戏时间，秒），并打 Warning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.1"))
	float MaxDuration = 5.f;

	/** 没有动画时，动作开始立即按顺序触发的关键帧（例如“此刻隐藏场景模型”） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TArray<FName> FallbackKeyframes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Restrictions")
	bool bBlockMove = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Restrictions")
	bool bBlockParkour = true;

	/** 禁止再次交互，避免动作期间重复触发场景物体 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Restrictions")
	bool bBlockInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Restrictions")
	bool bInvulnerable = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAOnCharacterActionRequested, FGameplayTag, ActionTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTAOnCharacterActionKeyframe, FGameplayTag, ActionTag, FName, KeyName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTAOnCharacterActionFinished, FGameplayTag, ActionTag, bool, bInterrupted);

/**
 * 角色动作接口（拿起 / 放下文本、塞入日记本等），把玩法和 PaperZD 动画解耦。
 *
 * 流程：PlayAction 广播 OnActionRequested。动画蓝图要播这个动作，就在收到事件时（同一帧内）调用 AcceptAction，
 * 播完调用 NotifyActionFinished，中途需要时调用 NotifyActionKeyframe。
 * 没有人认领时按配置的 FallbackDuration 自动完成，并在开始时立即触发 FallbackKeyframes，
 * 所以没有动画资源也能跑通整条流程。
 *
 * 数据操作（收集、销毁 Actor）不应等动画：调用方在交互帧做完，动作只是表现。
 * 同一时间只有一个动作；新动作会先打断旧动作。死亡、复活、EndPlay 时自动打断。
 * 动作期间按配置申请输入请求（优先级 ActionInputPriority，UI 打开时 UI 优先）和无敌保护，结束时释放。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTACharacterActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTACharacterActionComponent();

	/** 高于默认玩法输入（0），低于扫描（100）和所有界面 */
	static constexpr int32 ActionInputPriority = 50;

	using FOnFinished = TFunction<void(bool bInterrupted)>;

	/** 开始动作。正在进行的动作先被打断。OnFinished 一定会被调用且只调用一次（正常完成或被打断）。 */
	void PlayAction(FGameplayTag ActionTag, FOnFinished OnFinished = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Character Action", meta = (DisplayName = "Play Action", Categories = "Anim"))
	void PlayActionBP(FGameplayTag ActionTag) { PlayAction(ActionTag); }

	/** 动画蓝图在 OnActionRequested 中调用：认领该动作，之后必须调用 NotifyActionFinished */
	UFUNCTION(BlueprintCallable, Category = "Character Action", meta = (Categories = "Anim"))
	void AcceptAction(FGameplayTag ActionTag);

	/** 动画蓝图：动作的某个时刻到了（例如“此刻隐藏场景模型”） */
	UFUNCTION(BlueprintCallable, Category = "Character Action", meta = (Categories = "Anim"))
	void NotifyActionKeyframe(FGameplayTag ActionTag, FName KeyName);

	/** 动画蓝图：动作播完 */
	UFUNCTION(BlueprintCallable, Category = "Character Action", meta = (Categories = "Anim"))
	void NotifyActionFinished(FGameplayTag ActionTag);

	/** 立即结束当前动作，OnFinished 收到 bInterrupted = true。没有动作时无副作用。 */
	UFUNCTION(BlueprintCallable, Category = "Character Action")
	void InterruptAction();

	UFUNCTION(BlueprintPure, Category = "Character Action")
	bool IsActionActive() const { return ActiveTag.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Character Action")
	FGameplayTag GetActiveAction() const { return ActiveTag; }

	/** 当前动作是否由动画蓝图认领 */
	UFUNCTION(BlueprintPure, Category = "Character Action")
	bool IsActionAccepted() const { return bAccepted; }

	/** 动画蓝图订阅：收到后若要播放，同一帧内调用 AcceptAction */
	UPROPERTY(BlueprintAssignable, Category = "Character Action")
	FTAOnCharacterActionRequested OnActionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Character Action")
	FTAOnCharacterActionKeyframe OnActionKeyframe;

	UPROPERTY(BlueprintAssignable, Category = "Character Action")
	FTAOnCharacterActionFinished OnActionFinished;

	/** 每种动作的配置；没有配置的 Tag 使用 DefaultConfig */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Action")
	TArray<FTACharacterActionConfig> ActionConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Action")
	FTACharacterActionConfig DefaultConfig;

	const FTACharacterActionConfig& FindConfig(FGameplayTag ActionTag) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOwnerDeathOrRespawn();

	void Finish(bool bInterrupted);
	void HandleTimerElapsed();
	void AcquireRestrictions(const FTACharacterActionConfig& Config);
	void ReleaseRestrictions();

	FGameplayTag ActiveTag;
	FOnFinished ActiveOnFinished;
	bool bAccepted = false;
	FTAGameplayTimerHandle TimerHandle;

	TWeakObjectPtr<AThe_AwakeningPlayerController> InputRequestController;
	FTAInputRouter::FHandle InputRequestHandle = 0;
	FTAHealthProtectionHandle InvulnerableHandle;
};
