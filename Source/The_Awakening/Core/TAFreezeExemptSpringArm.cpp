#include "Core/TAFreezeExemptSpringArm.h"
#include "Engine/World.h"

void UTAFreezeExemptSpringArm::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaTime, TickType, TickFunction);
}
