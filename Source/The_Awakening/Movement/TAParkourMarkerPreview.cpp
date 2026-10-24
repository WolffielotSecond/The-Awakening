#include "Movement/TAParkourMarker.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Components/ChildActorComponent.h"

void ATAParkourMarker::HideLandingAnchor()
{
	for (bool bLower : {false, true})
	{
		if (auto* Target = GetLandingTargetComponent(bLower))
		{
			Target->SetHiddenInGame(true, false);
			if (AActor* Child = Target->GetChildActor()) Child->SetActorHiddenInGame(true);
		}
		if (auto* Prompt = GetLandingPrompt(bLower)) Prompt->SetHiddenInGame(false);
	}
}
void ATAParkourMarker::SyncPromptType()
{
	if (auto* Upper = GetLandingPrompt(false)) Upper->PromptType = ETAPromptType::ParkourJump;
	if (auto* Lower = GetLandingPrompt(true)) Lower->PromptType = ETAPromptType::ParkourDrop;
}
