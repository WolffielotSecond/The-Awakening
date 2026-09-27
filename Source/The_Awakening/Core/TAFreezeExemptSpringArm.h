#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "TAFreezeExemptSpringArm.generated.h"

/** Camera lag must keep using world delta even when its owning character is frozen. */
UCLASS()
class THE_AWAKENING_API UTAFreezeExemptSpringArm : public USpringArmComponent
{
	GENERATED_BODY()
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
};
