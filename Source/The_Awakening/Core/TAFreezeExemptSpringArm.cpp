#include "Core/TAFreezeExemptSpringArm.h"
#include "Engine/World.h"

void UTAFreezeExemptSpringArm::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaTime, TickType, TickFunction);
}

void UTAFreezeExemptSpringArm::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime)
{
	if (bSkipLagOnce)
	{
		bSkipLagOnce = false;
		// 关掉 Lag 更新一次，引擎会把上一帧位置记成当前位置，之后的 Lag 从新位置开始
		Super::UpdateDesiredArmLocation(bDoTrace, false, false, DeltaTime);
		return;
	}
	Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);
}
