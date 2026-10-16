// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/TAAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "Core/TAGameplayTags.h"
#include "Health/TADamageTypes.h"
#include "Health/TAHealthComponent.h"

UTAAttributeSet::UTAAttributeSet()
{
	// 初始默认值（后续也可以用 GameplayEffect 来初始化）
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitAttackMultiplier(1.0f);
	InitDefense(0.0f);
	InitDefenseBonus(0.0f);
	InitIncomingDamage(0.0f);
}

void UTAAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UTAAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTAAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTAAttributeSet, AttackMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTAAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTAAttributeSet, DefenseBonus, COND_None, REPNOTIFY_Always);
}

void UTAAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTAAttributeSet, Health, OldHealth);
}

void UTAAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTAAttributeSet, MaxHealth, OldMaxHealth);
}

void UTAAttributeSet::OnRep_AttackMultiplier(const FGameplayAttributeData& OldAttackMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTAAttributeSet, AttackMultiplier, OldAttackMultiplier);
}

void UTAAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTAAttributeSet, Defense, OldDefense);
}

void UTAAttributeSet::OnRep_DefenseBonus(const FGameplayAttributeData& OldDefenseBonus)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTAAttributeSet, DefenseBonus, OldDefenseBonus);
}

namespace
{
	// 当前值和基础值共用同一套限制，ApplyModToAttribute / SetNumericAttributeBase 等绕过 GE 的路径也受约束。
	void ClampAttributeValue(const UTAAttributeSet& Set, const FGameplayAttribute& Attribute, float& NewValue)
	{
		if (Attribute == UTAAttributeSet::GetMaxHealthAttribute())
		{
			// 防止 MaxHealth 小于 1
			NewValue = FMath::Max(NewValue, 1.f);
		}
		else if (Attribute == UTAAttributeSet::GetHealthAttribute())
		{
			NewValue = FMath::Clamp(NewValue, 0.f, Set.GetMaxHealth());
		}
	}
}

void UTAAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttributeValue(*this, Attribute, NewValue);
}

void UTAAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttributeValue(*this, Attribute, NewValue);
}

void UTAAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// MaxHealth 变小时同步压低 Health
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			ASC->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
}

void UTAAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Incoming = GetIncomingDamage();
		SetIncomingDamage(0.f);

		// 死亡判定不在这里做：统一由生命组件监听 Health 变化判定（任何改 Health 的路径都会经过）。
		const UAbilitySystemComponent& TargetASC = Data.Target;
		if (TargetASC.HasMatchingGameplayTag(TAGameplayTags::State_Dead))
		{
			// 死亡状态下所有伤害（包括强制伤害）都忽略
			return;
		}

		FGameplayTagContainer DamageTags;
		Data.EffectSpec.GetAllAssetTags(DamageTags);
		const ETADamageType Type = FTADamageCalculator::GetTypeFromTags(DamageTags);
		const FGameplayTag Source = FTADamageCalculator::GetSourceFromTags(DamageTags);
		// 保护和修改器存放在目标的生命组件上；没有生命组件的 ASC 只按状态 Tag 判断保护、没有修改器
		UTAHealthComponent* HealthComponent = UTAHealthComponent::FindHealthComponent(TargetASC.GetAvatarActor());

		if (Type != ETADamageType::Forced)
		{
			const bool bBlocked = HealthComponent ? HealthComponent->IsDamageBlocked(Source)
				: TargetASC.HasMatchingGameplayTag(TAGameplayTags::State_Invulnerable);
			if (bBlocked)
			{
				return;
			}
		}

		FTADamageCalculationInput Input;
		Input.Amount = Incoming;
		Input.Type = Type;
		Input.DamageTags = DamageTags;
		Input.Defense = GetDefense();
		Input.DefenseBonus = GetDefenseBonus();
		const TArray<FTADamageModifier> Modifiers = HealthComponent ? HealthComponent->GetActiveDamageModifiers() : TArray<FTADamageModifier>();
		const float FinalDamage = FTADamageCalculator::Compute(Input, Modifiers);

		const float Current = GetHealth();
		float NewHealth = Current - FinalDamage;
		if (Type != ETADamageType::Forced)
		{
			const bool bUndying = HealthComponent ? HealthComponent->HasProtection(ETAHealthProtection::Undying)
				: TargetASC.HasMatchingGameplayTag(TAGameplayTags::State_Undying);
			if (bUndying)
			{
				// 不会死：最低留 1 血；本来就低于 1 时不往上抬
				NewHealth = FMath::Max(NewHealth, FMath::Min(1.f, Current));
			}
		}

		if (HealthComponent)
		{
			// 先记录本次伤害（类型、来源、实际扣血），Health 变化与死亡事件里就能查到
			HealthComponent->NotifyDamageResolved(Current - FMath::Max(NewHealth, 0.f), Type, Source);
		}
		SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));
		return;
	}

	// 生命值变化后进行钳制
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}