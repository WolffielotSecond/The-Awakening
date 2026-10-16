#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "TADamageTypes.generated.h"

/** 伤害通道。 */
UENUM(BlueprintType)
enum class ETADamageType : uint8
{
	/** 完整公式：易伤 → 防御 → 减伤。受保护影响。 */
	Normal,
	/** 真实伤害：X × 真伤倍率，跳过易伤、防御、减伤。受保护影响。小游戏伤害属于此类。 */
	TrueDamage UMETA(DisplayName = "True"),
	/** 强制伤害：直接扣 Health，忽略所有修改器和保护。 */
	Forced
};

/** 伤害修改器种类。数值类相加，百分比 / 倍率类相乘。 */
UENUM(BlueprintType)
enum class ETADamageModifierKind : uint8
{
	/** 数值易伤：D1 = X + Σ值。 */
	FlatVulnerability,
	/** 百分比易伤：D2 = D1 × Π(1 + 值)。0.2 表示 +20%。 */
	PercentVulnerability,
	/** 固定值减伤：D4 = max(0, D3 − Σ值)。 */
	FlatReduction,
	/** 百分比减伤：最终 = D4 × Π(1 − 值)。0.3 表示 −30%；30% 与 20% 叠加为 ×0.7×0.8。 */
	PercentReduction,
	/** 真伤倍率：真实伤害 = X × Π值。0.5 表示减半。 */
	TrueDamageMultiplier
};

/** 一条伤害修改器。条件匹配本次伤害的类型 Tag 与来源 Tag，条件为空时对所有伤害生效。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTADamageModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ETADamageModifierKind Kind = ETADamageModifierKind::PercentReduction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Value = 0.f;

	/** 例如 RequireTags = Damage.Source.Enemy：只对敌人伤害生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (Categories = "Damage"))
	FGameplayTagRequirements Condition;

	/** > 0：临时修改器，从添加时开始按游戏时间计时，到期自动移除，复活时清除。<= 0：永久，需持有者移除。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (Units = "s"))
	float Duration = 0.f;

	bool IsTemporary() const { return Duration > 0.f; }
	bool Matches(const FGameplayTagContainer& DamageTags) const { return Condition.RequirementsMet(DamageTags); }
};

/** 修改器句柄。0 表示无效。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTADamageModifierHandle
{
	GENERATED_BODY()

	uint64 Id = 0;
	bool IsValid() const { return Id != 0; }
	bool operator==(const FTADamageModifierHandle& Other) const { return Id == Other.Id; }
};

/** 保护种类。无敌高于不会死；两者都挡不住强制伤害。 */
UENUM(BlueprintType)
enum class ETAHealthProtection : uint8
{
	/** 普通 / 真实伤害完全无效（可按来源放行）。 */
	Invulnerable,
	/** 普通 / 真实伤害照常扣血，但 Health 不会低于 1。 */
	Undying
};

/** 保护句柄。0 表示无效。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAHealthProtectionHandle
{
	GENERATED_BODY()

	uint64 Id = 0;
	bool IsValid() const { return Id != 0; }
	bool operator==(const FTAHealthProtectionHandle& Other) const { return Id == Other.Id; }
};

/** 一次伤害结算的输入。 */
struct THE_AWAKENING_API FTADamageCalculationInput
{
	float Amount = 0.f;
	ETADamageType Type = ETADamageType::Normal;
	/** 类型 Tag + 来源 Tag，用于匹配修改器条件。 */
	FGameplayTagContainer DamageTags;
	/** 角色基础属性，不参与 Tag 匹配。 */
	float Defense = 0.f;
	float DefenseBonus = 0.f;
};

/** 伤害公式（纯函数，无副作用）。 */
struct THE_AWAKENING_API FTADamageCalculator
{
	/**
	 * 普通：D1 = X + Σ数值易伤；D2 = D1 × Π(1+百分比易伤)；D3 = max(0, D2 − 防御×(1+防御加成))；
	 *       D4 = max(0, D3 − Σ固定减伤)；最终 = D4 × Π(1−百分比减伤)
	 * 真实：X × Π真伤倍率
	 * 强制：X
	 * 每一条条件匹配的修改器都会计入；结果不小于 0，保留小数。
	 */
	static float Compute(const FTADamageCalculationInput& Input, TConstArrayView<FTADamageModifier> Modifiers);

	static FGameplayTag GetTypeTag(ETADamageType Type);
	/** 从 Spec Tag 中解析类型；缺省视为 Normal。 */
	static ETADamageType GetTypeFromTags(const FGameplayTagContainer& Tags);
	/** 取 Damage.Source 下的第一个来源 Tag；没有则返回空 Tag。 */
	static FGameplayTag GetSourceFromTags(const FGameplayTagContainer& Tags);
	/** 合法来源：Damage.Source 的子 Tag（不能是 Damage.Source 本身）。 */
	static bool IsValidSourceTag(const FGameplayTag& Source);
};
