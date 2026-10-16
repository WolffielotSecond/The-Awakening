// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TAAttributeSet.generated.h"

// 用于简化属性访问的宏
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 *
 */
UCLASS()
class THE_AWAKENING_API UTAAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTAAttributeSet();

	// -------------------- Health --------------------
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UTAAttributeSet, Health)

		UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UTAAttributeSet, MaxHealth)

		// -------------------- Combat --------------------
		UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackMultiplier)
	FGameplayAttributeData AttackMultiplier;
	ATTRIBUTE_ACCESSORS(UTAAttributeSet, AttackMultiplier)

	/** 防御力：角色基础属性，普通伤害在易伤之后、减伤之前减去 防御力 × (1 + 防御力加成)。不参与伤害 Tag 匹配。 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UTAAttributeSet, Defense)

	/** 防御力加成：0.5 表示防御力按 150% 计算。 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_DefenseBonus)
	FGameplayAttributeData DefenseBonus;
	ATTRIBUTE_ACCESSORS(UTAAttributeSet, DefenseBonus)

	// -------------------- Meta --------------------
	/** 本次 GE 执行的待结算伤害。不复制、不存档，结算后立即清零。 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UTAAttributeSet, IncomingDamage)

		virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_AttackMultiplier(const FGameplayAttributeData& OldAttackMultiplier);

	UFUNCTION()
	virtual void OnRep_Defense(const FGameplayAttributeData& OldDefense);

	UFUNCTION()
	virtual void OnRep_DefenseBonus(const FGameplayAttributeData& OldDefenseBonus);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};