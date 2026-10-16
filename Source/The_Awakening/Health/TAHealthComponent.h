#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Health/TADamageTypes.h"
#include "Core/TATimeSubsystem.h"
#include "TAHealthComponent.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTAHealthChangedSignature, float, Current, float, Max, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTAHealthLifecycleSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTADamageTakenSignature, float, Amount, ETADamageType, Type, FGameplayTag, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTADamageModifierExpiredSignature, FTADamageModifierHandle, Handle);

/**
 * 生命状态的唯一对外窗口：监听 ASC 上的 Health / MaxHealth，向 HUD、动画广播事件。
 * ASC 在 PlayerState 上，由角色在 InitAbilityActorInfo 完成后调用 InitializeWithASC 绑定。
 */
UCLASS(ClassGroup = (TheAwakening), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTAHealthComponent();

	/** 绑定 ASC 的属性变化委托。重复绑定同一个 ASC 无副作用；换 ASC 时先解绑旧的。 */
	void InitializeWithASC(UAbilitySystemComponent* InASC);
	/** 解绑属性委托。EndPlay 自动调用。 */
	void UninitializeFromASC();

	UFUNCTION(BlueprintPure, Category = "Health")
	static UTAHealthComponent* FindHealthComponent(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const;
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const;
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const;
	/** Health / MaxHealth，未绑定 ASC 时为 0。 */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthNormalized() const;

	UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	/** 复活收尾，由 UTARespawnSubsystem 调用：移除 State.Dead、恢复死亡前的扫描开关、广播 OnRespawned。 */
	void FinishRespawn();

	/** Health 或 MaxHealth 变化（含绑定时的一次初始同步，Delta 为 0）。Delta < 0 时可播受击反馈。 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FTAHealthChangedSignature OnHealthChanged;

	/** 进入死亡状态的一帧（死亡清理完成后、进入复活流程前）。 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FTAHealthLifecycleSignature OnDeathStarted;

	/** 复活完成、控制权交还时。由复活流程广播（实施步骤 4）。 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FTAHealthLifecycleSignature OnRespawned;

	// -------------------- 伤害修改器 --------------------
	/**
	 * 添加一条伤害修改器。Owner 是持有者：只能由持有者用句柄移除；Owner 销毁后该修改器自动失效。
	 * Duration > 0 为临时修改器，按游戏时间计时，到期自动移除并广播 OnDamageModifierExpired。
	 */
	UFUNCTION(BlueprintCallable, Category = "Health|Damage")
	FTADamageModifierHandle AddDamageModifier(const FTADamageModifier& Modifier, UObject* Owner);
	UFUNCTION(BlueprintCallable, Category = "Health|Damage")
	bool RemoveDamageModifier(UPARAM(ref) FTADamageModifierHandle& Handle);
	/** 复活时调用：只清除临时修改器，永久修改器保留。 */
	UFUNCTION(BlueprintCallable, Category = "Health|Damage")
	void ClearTemporaryDamageModifiers();
	/** 当前生效的全部修改器（持有者已销毁的不计入）。 */
	TArray<FTADamageModifier> GetActiveDamageModifiers() const;
	UFUNCTION(BlueprintPure, Category = "Health|Damage")
	int32 GetActiveDamageModifierCount() const { return GetActiveDamageModifiers().Num(); }

	UPROPERTY(BlueprintAssignable, Category = "Health|Damage")
	FTADamageModifierExpiredSignature OnDamageModifierExpired;

	// -------------------- 保护 --------------------
	/**
	 * 统一保护入口。Owner 持有保护，只能由持有者用句柄移除。同时在 ASC 上维护 State.Invulnerable / State.Undying，
	 * 供其他系统查询；不要直接加减这两个 Tag。
	 * @param AllowedDamageSources 仅对无敌有效：来源匹配其中任一 Tag 的伤害不被这条无敌挡住（例如小游戏放行 Damage.Source.Puzzle）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Health|Protection")
	FTAHealthProtectionHandle AddProtection(ETAHealthProtection Kind, UObject* Owner,
		UPARAM(meta = (Categories = "Damage.Source")) FGameplayTagContainer AllowedDamageSources);
	UFUNCTION(BlueprintCallable, Category = "Health|Protection")
	bool RemoveProtection(UPARAM(ref) FTAHealthProtectionHandle& Handle);
	UFUNCTION(BlueprintPure, Category = "Health|Protection")
	bool HasProtection(ETAHealthProtection Kind) const;
	/** 普通 / 真实伤害是否被无敌挡住：存在任一条不放行该来源的无敌保护。 */
	bool IsDamageBlocked(const FGameplayTag& Source) const;

	// -------------------- 伤害事件 --------------------
	/** 由 AttributeSet 在扣血前调用，记录并广播本次实际伤害。 */
	void NotifyDamageResolved(float Amount, ETADamageType Type, FGameplayTag Source);
	/** 实际扣血后的伤害事件（被保护完全挡住的不广播）。 */
	UPROPERTY(BlueprintAssignable, Category = "Health|Damage")
	FTADamageTakenSignature OnDamageTaken;
	/** 最近一次实际生效的伤害来源（例如死亡原因）。 */
	UFUNCTION(BlueprintPure, Category = "Health|Damage")
	FGameplayTag GetLastDamageSource() const { return LastDamageSource; }
	UFUNCTION(BlueprintPure, Category = "Health|Damage")
	ETADamageType GetLastDamageType() const { return LastDamageType; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	virtual void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	/** 濒死拦截预留：以后肾上腺素 QTE、回光返照从这里接入。返回 true 则本次不进入死亡。 */
	virtual bool ShouldInterceptDeath() const { return false; }

	/** Health 从 >0 变为 <=0 且当前未死亡时调用一次，按固定顺序执行死亡清理。 */
	void HandleDeath();

private:
	// 只撤销自己做过的事：State.Dead 只移除自己加的那一层；扫描只恢复成死亡前的值。
	bool bAppliedDeathTag = false;
	TWeakObjectPtr<class UTAScanningComponent> ScanDisabledOnDeath;
	bool bScanEnabledBeforeDeath = true;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

	struct FModifierEntry
	{
		uint64 Id = 0;
		FTADamageModifier Modifier;
		TWeakObjectPtr<UObject> Owner;
		FTAGameplayTimerHandle ExpiryTimer;
		bool IsActive() const { return Owner.IsValid(); }
	};
	struct FProtectionEntry
	{
		uint64 Id = 0;
		ETAHealthProtection Kind = ETAHealthProtection::Invulnerable;
		TWeakObjectPtr<UObject> Owner;
		FGameplayTagContainer AllowedSources;
		bool IsActive() const { return Owner.IsValid(); }
	};

	void ExpireDamageModifier(uint64 Id);
	void ClearModifierTimer(FModifierEntry& Entry);
	/** 移除持有者已销毁的条目，并把 ASC 上的保护 Tag 同步成当前生效条数。 */
	void PruneAndSyncProtectionTags();
	int32 CountActiveProtections(ETAHealthProtection Kind) const;

	TArray<FModifierEntry> Modifiers;
	TArray<FProtectionEntry> Protections;
	uint64 NextEntryId = 0;
	// 已加到当前 ASC 上的保护 Tag 层数，只增减自己这部分
	int32 AppliedInvulnerableTags = 0;
	int32 AppliedUndyingTags = 0;

	FGameplayTag LastDamageSource;
	ETADamageType LastDamageType = ETADamageType::Normal;
};
