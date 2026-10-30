#include "Interaction/TAInteractionQuery.h"
#include "Interaction/TAInteractable.h"
#include "Interaction/TAInteractionBehaviorComponent.h"
#include "Inventory/TAWorldItem.h"
#include "Camera/TARoomRegion.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AActor* FTAInteractionQuery::FindBestInteractable(const APawn* Pawn, float Radius)
{
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TAInteractionQuery), false, Pawn);
	const FVector Origin = Pawn->GetActorLocation();
	if (!World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params))
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Actor = Result.GetActor();
		if (!Actor || !Actor->Implements<UTAInteractable>() || Cast<ATAWorldItem>(Actor))
		{
			continue;
		}
		if (UTARoomRegistrySubsystem::IsActorInBlackoutRoom(Actor)) continue;
		if (const auto* Behavior = Actor->FindComponentByClass<UTAInteractionBehaviorComponent>(); Behavior && !Behavior->IsSelectable(const_cast<APawn*>(Pawn))) continue;
		if (!ITAInteractable::Execute_CanInteract(Actor, const_cast<APawn*>(Pawn)))
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Actor;
		}
	}
	return Best;
}
