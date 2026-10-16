#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TAGameplayEffect_Damage.generated.h"

/**
 * 统一伤害 GE：Instant，一条 IncomingDamage Additive Modifier，数值取 SetByCaller Data.Damage。
 * 纯 C++ 定义，不需要在编辑器里建资产；调用方统一走 UTADamageLibrary::ApplyDamage。
 */
UCLASS()
class THE_AWAKENING_API UTAGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTAGameplayEffect_Damage();
};
