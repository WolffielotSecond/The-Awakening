#include "Health/TADamageTypes.h"
#include "Core/TAGameplayTags.h"

float FTADamageCalculator::Compute(const FTADamageCalculationInput& Input, TConstArrayView<FTADamageModifier> Modifiers)
{
	const float X = FMath::Max(0.f, Input.Amount);
	if (Input.Type == ETADamageType::Forced)
	{
		return X;
	}

	float FlatVulnerability = 0.f;
	float VulnerabilityFactor = 1.f;
	float FlatReduction = 0.f;
	float ReductionFactor = 1.f;
	float TrueFactor = 1.f;
	for (const FTADamageModifier& Modifier : Modifiers)
	{
		if (!Modifier.Matches(Input.DamageTags))
		{
			continue;
		}
		switch (Modifier.Kind)
		{
		case ETADamageModifierKind::FlatVulnerability:    FlatVulnerability += Modifier.Value; break;
		case ETADamageModifierKind::PercentVulnerability: VulnerabilityFactor *= 1.f + Modifier.Value; break;
		case ETADamageModifierKind::FlatReduction:        FlatReduction += Modifier.Value; break;
		case ETADamageModifierKind::PercentReduction:     ReductionFactor *= 1.f - Modifier.Value; break;
		case ETADamageModifierKind::TrueDamageMultiplier: TrueFactor *= Modifier.Value; break;
		}
	}

	if (Input.Type == ETADamageType::TrueDamage)
	{
		return FMath::Max(0.f, X * TrueFactor);
	}

	const float D1 = X + FlatVulnerability;
	const float D2 = D1 * VulnerabilityFactor;
	const float D3 = FMath::Max(0.f, D2 - Input.Defense * (1.f + Input.DefenseBonus));
	const float D4 = FMath::Max(0.f, D3 - FlatReduction);
	return FMath::Max(0.f, D4 * ReductionFactor);
}

FGameplayTag FTADamageCalculator::GetTypeTag(ETADamageType Type)
{
	switch (Type)
	{
	case ETADamageType::TrueDamage:   return TAGameplayTags::Damage_Type_True;
	case ETADamageType::Forced: return TAGameplayTags::Damage_Type_Forced;
	default:                    return TAGameplayTags::Damage_Type_Normal;
	}
}

ETADamageType FTADamageCalculator::GetTypeFromTags(const FGameplayTagContainer& Tags)
{
	if (Tags.HasTagExact(TAGameplayTags::Damage_Type_Forced)) return ETADamageType::Forced;
	if (Tags.HasTagExact(TAGameplayTags::Damage_Type_True)) return ETADamageType::TrueDamage;
	return ETADamageType::Normal;
}

FGameplayTag FTADamageCalculator::GetSourceFromTags(const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		if (IsValidSourceTag(Tag)) return Tag;
	}
	return FGameplayTag();
}

bool FTADamageCalculator::IsValidSourceTag(const FGameplayTag& Source)
{
	const FGameplayTag Root = TAGameplayTags::Damage_Source;
	return Source.IsValid() && Source != Root && Source.MatchesTag(Root);
}
