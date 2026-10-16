#include "Movement/TAParkourMarker.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Components/ChildActorComponent.h"

void ATAParkourMarker::HideLandingAnchor()
{
	// The landing actor is only an anchor. Never reveal its sprite/arrow in game.
	if (LandingTargetComponent)
	{
		LandingTargetComponent->SetHiddenInGame(true, false);
		if (AActor* Child = LandingTargetComponent->GetChildActor()) Child->SetActorHiddenInGame(true);
	}
	// 提示锚点挂在落点下，但不继承上面的隐藏；它的显隐只由统一提示系统控制
	if (PromptWidget) PromptWidget->SetHiddenInGame(false);
}

void ATAParkourMarker::SyncPromptType()
{
	if (PromptWidget) PromptWidget->PromptType = GetPromptType();
}
