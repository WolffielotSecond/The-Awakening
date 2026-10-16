#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TARespawnSettings.generated.h"

/** 复活流程参数。项目设置 → Game → The Awakening Respawn 中可改，保存在 DefaultGame.ini。时长均按游戏时间计算。 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "The Awakening Respawn"))
class THE_AWAKENING_API UTARespawnSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 死亡后到开始黑屏的时间（播放死亡动画、空中死亡落地）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float DeathDelay = 1.5f;

	/** 淡出到全黑的时间。以后商人拖回的演出插在这一段。 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float FadeOutTime = 0.5f;

	/** 复位后从全黑淡入的时间。 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float FadeInTime = 0.5f;

	/** 复活后的无敌时间，防止在复活点旁边被连续击杀。强制伤害不受影响。 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float RespawnInvulnerableTime = 2.f;

	/** 复活时 Health = MaxHealth × 该比例。设计上没有死亡惩罚，默认回满。 */
	UPROPERTY(Config, EditAnywhere, Category = "Health", meta = (ClampMin = "0.01", ClampMax = "1"))
	float RespawnHealthRatio = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Presentation")
	FLinearColor FadeColor = FLinearColor::Black;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
