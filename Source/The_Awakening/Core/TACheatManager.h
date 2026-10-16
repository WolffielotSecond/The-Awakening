#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Health/TADamageTypes.h"
#include "TACheatManager.generated.h"

class UAbilitySystemComponent;
class UTAHealthComponent;

/**
 * 项目调试命令。CheatManager 只在允许作弊时创建（PIE / 开发版），Shipping 包自动没有。
 * 输出只写日志，属于开发者文本，不接本地化。所有伤害来源为 Damage.Source.Console。
 */
UCLASS()
class THE_AWAKENING_API UTACheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** TADamage <Amount>：普通伤害，验证修改器、防御和保护。 */
	UFUNCTION(Exec)
	void TADamage(float Amount);

	/** TATrueDamage <Amount>：真实伤害。 */
	UFUNCTION(Exec)
	void TATrueDamage(float Amount);

	/** TAKill：强制伤害，扣掉全部 Health，任何保护都挡不住。 */
	UFUNCTION(Exec)
	void TAKill();

	/** TAGod：切换一条永久无敌保护（不挡强制伤害）。 */
	UFUNCTION(Exec)
	void TAGod();

	/** TACheckpoint <Id>：直接设为当前复活点，不用走过去交互。 */
	UFUNCTION(Exec)
	void TACheckpoint(FName CheckpointId);

	bool IsGodModeEnabled() const { return GodProtection.IsValid(); }

private:
	UAbilitySystemComponent* GetPlayerASC() const;
	UTAHealthComponent* GetPlayerHealth() const;
	void ApplyConsoleDamage(float Amount, ETADamageType Type, const TCHAR* Command);

	// 只持有自己这一条保护，关闭时只移除它，不影响对话、小游戏、复活保护等其他来源
	FTAHealthProtectionHandle GodProtection;
	TWeakObjectPtr<UTAHealthComponent> GodHealth;
};
