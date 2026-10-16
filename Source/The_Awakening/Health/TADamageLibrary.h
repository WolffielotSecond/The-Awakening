#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Health/TADamageTypes.h"
#include "TADamageLibrary.generated.h"

/** 统一伤害入口。敌人、机关、杀死体积、小游戏、调试命令都只调这里；保护、修改器、防御只在结算时处理。 */
UCLASS()
class THE_AWAKENING_API UTADamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 对 Target 的 ASC 应用一次伤害。
	 * @param Instigator 伤害来源对象，可为空，只写入 EffectContext。
	 * @param DamageType Normal 走完整公式；True 只乘真伤倍率；Forced 直接扣血，忽略修改器和保护。
	 * @param Source     必填，Damage.Source 的子 Tag（例如 Damage.Source.Enemy）。缺失或不合法时拒绝并打 Warning。
	 * @return GE 是否成功应用。被保护挡住、或死亡状态下被忽略的伤害也算应用成功，是否扣血看 Health 变化 / OnDamageTaken。
	 */
	UFUNCTION(BlueprintCallable, Category = "Health|Damage", meta = (DefaultToSelf = "Instigator"))
	static bool ApplyDamage(AActor* Instigator, AActor* Target, float Amount, ETADamageType DamageType,
		UPARAM(meta = (Categories = "Damage.Source")) FGameplayTag Source);
};
