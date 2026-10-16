#include "Health/TADamageLibrary.h"
#include "Health/TAGameplayEffect_Damage.h"
#include "Health/TARollbackSubsystem.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Core/TAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogTADamage, Log, All);

bool UTADamageLibrary::ApplyDamage(AActor* Instigator, AActor* Target, float Amount, ETADamageType DamageType, FGameplayTag Source)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogTADamage, Warning, TEXT("ApplyDamage: %s has no AbilitySystemComponent."), *GetNameSafe(Target));
		return false;
	}
	if (!FTADamageCalculator::IsValidSourceTag(Source))
	{
		UE_LOG(LogTADamage, Warning, TEXT("ApplyDamage: '%s' is not a Damage.Source.* tag; every damage needs a source."), *Source.ToString());
		return false;
	}
	if (Amount < 0.f)
	{
		UE_LOG(LogTADamage, Warning, TEXT("ApplyDamage: negative amount %.2f ignored; use healing instead."), Amount);
		return false;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(Instigator, Instigator);

	const FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(UTAGameplayEffect_Damage::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(TAGameplayTags::Data_Damage, Amount);
	Spec.Data->AddDynamicAssetTag(FTADamageCalculator::GetTypeTag(DamageType));
	Spec.Data->AddDynamicAssetTag(Source);

	// 对话 / 小游戏进行中：记录扣血前的 Health，死亡回滚时恢复
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(Target); Rollback && Rollback->IsRecording())
	{
		const float HealthBefore = TargetASC->GetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute());
		Rollback->RecordUndo(TEXT("Damage.Health"), [WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(TargetASC), HealthBefore]
		{
			if (UAbilitySystemComponent* ASC = WeakASC.Get())
			{
				ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), HealthBefore);
			}
		});
	}

	const FActiveGameplayEffectHandle Handle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return Handle.WasSuccessfullyApplied();
}
