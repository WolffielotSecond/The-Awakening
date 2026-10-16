#include "Puzzle/TAPuzzleRewards.h"
#include "Core/TAPlayerState.h"
#include "Core/TAGameplayTags.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Health/TADamageLibrary.h"
#include "Health/TARollbackSubsystem.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"

namespace
{
	/** 记录 ASC 上全部 UTAAttributeSet 属性的基础值，回滚时整体恢复（覆盖即时 GE、回血、MaxHealth 变化等无法逐项反算的改动）。 */
	void RecordAttributeSnapshot(UTARollbackSubsystem* Rollback, UAbilitySystemComponent* ASC)
	{
		if (!Rollback || !Rollback->IsRecording() || !ASC) return;
		const TArray<FGameplayAttribute> Attributes = {
			UTAAttributeSet::GetMaxHealthAttribute(), UTAAttributeSet::GetHealthAttribute(), UTAAttributeSet::GetAttackMultiplierAttribute(),
			UTAAttributeSet::GetDefenseAttribute(), UTAAttributeSet::GetDefenseBonusAttribute() };
		TArray<float> Values;
		for (const FGameplayAttribute& Attribute : Attributes) Values.Add(ASC->GetNumericAttributeBase(Attribute));
		// MaxHealth 在 Health 之前恢复，避免 Health 被旧上限压低
		Rollback->RecordUndo(TEXT("Puzzle.Attributes"), [WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC), Attributes, Values]
		{
			if (UAbilitySystemComponent* Target = WeakASC.Get())
				for (int32 I = 0; I < Attributes.Num(); ++I) Target->SetNumericAttributeBase(Attributes[I], Values[I]);
		});
	}
}

int32 TAPuzzleRewards::ApplyDefault(APlayerController* Player, const FTAPuzzleEffect& E)
{
	if (!IsValid(Player)) return 0;
	UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(Player);
	if (E.Type == ETAPuzzleEffectType::Item)
	{
		UTAInventoryComponent* Inventory = Player->GetPawn() ? Player->GetPawn()->FindComponentByClass<UTAInventoryComponent>() : nullptr;
		const int32 Added = Inventory && E.Item && E.Amount > 0 ? Inventory->TryAddItem(E.Item, E.Amount) : 0;
		if (Added > 0 && Rollback && Rollback->IsRecording())
		{
			// 已知限制：撤销按物品定义移除，格子顺序会重排，武器耐久按移除时的实例计
			Rollback->RecordUndo(TEXT("Puzzle.Item"), [WeakInventory = TWeakObjectPtr<UTAInventoryComponent>(Inventory),
				WeakItem = TWeakObjectPtr<UTAItemDefinition>(E.Item), Added]
			{
				if (UTAInventoryComponent* Target = WeakInventory.Get()) Target->RemoveItem(WeakItem.Get(), Added);
			});
		}
		return Added;
	}
	ATAPlayerState* PS = Player->GetPlayerState<ATAPlayerState>();
	if (!PS) return 0;
	if (E.Type == ETAPuzzleEffectType::Money)
	{
		const int32 Before = PS->GetMoney(); PS->AddMoney(E.Amount);
		const int32 Delta = PS->GetMoney() - Before;
		if (Delta != 0 && Rollback && Rollback->IsRecording())
		{
			// 记录实际变化量（AddMoney 在 0 处截断），撤销时反向加回
			Rollback->RecordUndo(TEXT("Puzzle.Money"), [WeakPS = TWeakObjectPtr<ATAPlayerState>(PS), Delta]
			{
				if (ATAPlayerState* Target = WeakPS.Get()) Target->AddMoney(-Delta);
			});
		}
		return Delta;
	}
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	UTAAttributeSet* Attributes = PS->GetAttributeSet();
	if (!ASC || !Attributes) return 0;
	if (E.Type == ETAPuzzleEffectType::GameplayEffect && E.GameplayEffectClass)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(E.GameplayEffectClass, FMath::Max(1.f, E.EffectLevel), ASC->MakeEffectContext());
		if (!Spec.IsValid()) return 0;
		RecordAttributeSnapshot(Rollback, ASC);
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (Handle.IsValid() && Rollback && Rollback->IsRecording())
		{
			// 持续型 GE：撤销时移除效果本身（即时型由上面的属性快照恢复）
			Rollback->RecordUndo(TEXT("Puzzle.GameplayEffect"), [WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC), Handle]
			{
				if (UAbilitySystemComponent* Target = WeakASC.Get()) Target->RemoveActiveGameplayEffect(Handle);
			});
		}
		return Handle.WasSuccessfullyApplied() ? 1 : 0;
	}
	if (E.Type == ETAPuzzleEffectType::Health || E.Type == ETAPuzzleEffectType::MaxHealth)
	{
		const bool bMax = E.Type == ETAPuzzleEffectType::MaxHealth;
		const FGameplayAttribute Attribute = bMax ? UTAAttributeSet::GetMaxHealthAttribute() : UTAAttributeSet::GetHealthAttribute();
		const float Before = ASC->GetNumericAttribute(Attribute);
		if (!bMax && E.Amount < 0)
		{
			// 小游戏扣血是真实伤害，来源 Puzzle：小游戏期间的无敌会放行它，“不会死”保底 1 血
			// 目标用持有 ASC 的 PlayerState，没有 Pawn 时也能结算
			UTADamageLibrary::ApplyDamage(PS, PS, static_cast<float>(-E.Amount),
				ETADamageType::TrueDamage, TAGameplayTags::Damage_Source_Puzzle);
			return FMath::RoundToInt(ASC->GetNumericAttribute(Attribute) - Before);
		}
		RecordAttributeSnapshot(Rollback, ASC);
		const float Desired = bMax ? FMath::Max(1.f, Before + E.Amount) : FMath::Clamp(Before + E.Amount, 0.f, Attributes->GetMaxHealth());
		ASC->ApplyModToAttribute(Attribute, EGameplayModOp::Additive, Desired - Before);
		if (bMax && Attributes->GetHealth() > Attributes->GetMaxHealth())
			ASC->ApplyModToAttribute(UTAAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, Attributes->GetMaxHealth() - Attributes->GetHealth());
		return FMath::RoundToInt(ASC->GetNumericAttribute(Attribute) - Before);
	}
	return 0;
}
