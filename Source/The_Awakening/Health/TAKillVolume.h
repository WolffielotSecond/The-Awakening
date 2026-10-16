#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TAKillVolume.generated.h"

class UBoxComponent;

/**
 * 杀死体积：掉出地图、深渊。进入的、带生命组件的角色受到一次强制伤害（无视无敌、不会死和所有修改器）。
 * 范围用 Box 组件在关卡里拉伸。
 */
UCLASS()
class THE_AWAKENING_API ATAKillVolume : public AActor
{
	GENERATED_BODY()

public:
	ATAKillVolume();

	/** 强制伤害数值；默认远大于任何 MaxHealth。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kill Volume", meta = (ClampMin = "0"))
	float Damage = 1000000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kill Volume", meta = (Categories = "Damage.Source"))
	FGameplayTag DamageSource;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Volume;
};
