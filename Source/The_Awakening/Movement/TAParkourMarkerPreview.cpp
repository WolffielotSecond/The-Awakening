#include "Movement/TAParkourMarker.h"
#include "Movement/TAParkourComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"

void ATAParkourMarker::SetLandingPreviewVisible(bool bVisible)
{
	// The landing actor is only an anchor. Never reveal its sprite/arrow in game.
	if (LandingTargetComponent)
	{
		LandingTargetComponent->SetHiddenInGame(true, false);
		if (AActor* Child = LandingTargetComponent->GetChildActor()) Child->SetActorHiddenInGame(true);
	}
	// Keep the attached widget independent of the hidden anchor; prompt selection still controls Visibility.
	if (PromptWidget) PromptWidget->SetHiddenInGame(!bVisible);
}

void ATAParkourMarker::RefreshLandingPreview()
{
	const UTAParkourComponent* Parkour = CurrentOverlappingActor.IsValid()
		? CurrentOverlappingActor->FindComponentByClass<UTAParkourComponent>() : nullptr;
	SetLandingPreviewVisible(Parkour && Parkour->CanParkourToMarker(this));
}
