#include "Health/TAGameplayEffect_Damage.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Core/TAGameplayTags.h"

UTAGameplayEffect_Damage::UTAGameplayEffect_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAGameplayTags::Data_Damage;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UTAAttributeSet::GetIncomingDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}
