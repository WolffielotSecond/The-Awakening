#include "Puzzle/TAPuzzleRewards.h"
#include "Core/TAPlayerState.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Inventory/TAInventoryComponent.h"

int32 TAPuzzleRewards::ApplyDefault(APlayerController* Player, const FTAPuzzleEffect& E)
{
	if (!IsValid(Player)) return 0;
	if (E.Type == ETAPuzzleEffectType::Item)
	{
		UTAInventoryComponent* Inventory = Player->GetPawn() ? Player->GetPawn()->FindComponentByClass<UTAInventoryComponent>() : nullptr;
		return Inventory && E.Item && E.Amount > 0 ? Inventory->TryAddItem(E.Item, E.Amount) : 0;
	}
	ATAPlayerState* PS = Player->GetPlayerState<ATAPlayerState>();
	if (!PS) return 0;
	if (E.Type == ETAPuzzleEffectType::Money)
	{
		const int32 Before = PS->GetMoney(); PS->AddMoney(E.Amount); return PS->GetMoney() - Before;
	}
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	UTAAttributeSet* Attributes = PS->GetAttributeSet();
	if (!ASC || !Attributes) return 0;
	if (E.Type == ETAPuzzleEffectType::GameplayEffect && E.GameplayEffectClass)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(E.GameplayEffectClass, FMath::Max(1.f, E.EffectLevel), ASC->MakeEffectContext());
		if (!Spec.IsValid()) return 0;
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		return Handle.WasSuccessfullyApplied() ? 1 : 0;
	}
	if (E.Type == ETAPuzzleEffectType::Health || E.Type == ETAPuzzleEffectType::MaxHealth)
	{
		const bool bMax = E.Type == ETAPuzzleEffectType::MaxHealth;
		const FGameplayAttribute Attribute = bMax ? UTAAttributeSet::GetMaxHealthAttribute() : UTAAttributeSet::GetHealthAttribute();
		const float Before = ASC->GetNumericAttribute(Attribute);
		const float Desired = bMax ? FMath::Max(1.f, Before + E.Amount) : FMath::Clamp(Before + E.Amount, 0.f, Attributes->GetMaxHealth());
		ASC->ApplyModToAttribute(Attribute, EGameplayModOp::Additive, Desired - Before);
		if (bMax && Attributes->GetHealth() > Attributes->GetMaxHealth())
			ASC->ApplyModToAttribute(UTAAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, Attributes->GetMaxHealth() - Attributes->GetHealth());
		return FMath::RoundToInt(ASC->GetNumericAttribute(Attribute) - Before);
	}
	return 0;
}
