#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "TAFreezeExemptSpringArm.generated.h"

/** Camera lag must keep using world delta even when its owning character is frozen. */
UCLASS()
class THE_AWAKENING_API UTAFreezeExemptSpringArm : public USpringArmComponent
{
	GENERATED_BODY()
public:
	/** 下一次更新跳过位置 / 旋转 Lag，直接到位（复活传送用，避免镜头从死亡点飞过来）。 */
	void SkipLagOnce() { bSkipLagOnce = true; }
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;
private:
	bool bSkipLagOnce = false;
};
