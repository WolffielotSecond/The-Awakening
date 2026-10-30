#include "Camera/TARoomRegion.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

namespace
{
	bool BoxContainsPoint(const UBoxComponent* Box, const FVector& WorldPoint, float Margin)
	{
		if (!Box) return false;
		const FVector Local = Box->GetComponentTransform().InverseTransformPositionNoScale(WorldPoint);
		const FVector Extent = Box->GetScaledBoxExtent() + FVector(Margin);
		return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
	}

	UBoxComponent* CreateTriggerBox(AActor* Owner, FName Name, const FVector& Extent, const FColor& Color)
	{
		UBoxComponent* Box = Owner->CreateDefaultSubobject<UBoxComponent>(Name);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Box->ShapeColor = Color;
		return Box;
	}
}

ATARoomRegion::ATARoomRegion()
{
	PrimaryActorTick.bCanEverTick = false;
	RegionBox = CreateTriggerBox(this, TEXT("RegionBox"), FVector(200.f, 200.f, 150.f), FColor(80, 200, 255));
	RootComponent = RegionBox;
}

bool ATARoomRegion::ContainsPoint(const FVector& WorldPoint) const
{
	TInlineComponentArray<UBoxComponent*> Boxes(this);
	for (const UBoxComponent* Box : Boxes)
	{
		if (BoxContainsPoint(Box, WorldPoint, 0.f)) return true;
	}
	return false;
}

void ATARoomRegion::BeginPlay()
{
	Super::BeginPlay();
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->RegisterRegion(this);
}

void ATARoomRegion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->UnregisterRegion(this);
	Super::EndPlay(EndPlayReason);
}

ATARoomPortal::ATARoomPortal()
{
	PrimaryActorTick.bCanEverTick = false;
	PortalBox = CreateTriggerBox(this, TEXT("PortalBox"), FVector(60.f, 60.f, 120.f), FColor(255, 200, 60));
	RootComponent = PortalBox;
}

UTARoomRegistrySubsystem* UTARoomRegistrySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTARoomRegistrySubsystem>() : nullptr;
}

void UTARoomRegistrySubsystem::RegisterRegion(ATARoomRegion* Region)
{
	if (Region) Regions.AddUnique(Region);
	ApplyRoomVisibility(CurrentRegion.Get());
}

void UTARoomRegistrySubsystem::UnregisterRegion(ATARoomRegion* Region)
{
	Regions.RemoveAll([Region](const TWeakObjectPtr<ATARoomRegion>& Entry) { return !Entry.IsValid() || Entry.Get() == Region; });
	if (Region) Region->bBlackedOut = false;
	ApplyRoomVisibility(CurrentRegion.Get() == Region ? nullptr : CurrentRegion.Get());
}

ATARoomRegion* UTARoomRegistrySubsystem::ResolveRegion(const FVector& WorldPoint, const ATARoomRegion* Previous) const
{
	ATARoomRegion* Best = nullptr;
	for (const TWeakObjectPtr<ATARoomRegion>& Entry : Regions)
	{
		ATARoomRegion* Region = Entry.Get();
		if (!Region || !Region->ContainsPoint(WorldPoint)) continue;
		if (!Best || Region->Priority > Best->Priority)
		{
			Best = Region;
			continue;
		}
		if (Region->Priority < Best->Priority || Best == Previous) continue;
		if (Region == Previous || Region->RoomId.LexicalLess(Best->RoomId)) Best = Region;
	}
	return Best;
}

void UTARoomRegistrySubsystem::ApplyRoomVisibility(ATARoomRegion* ActiveRegion)
{
	CurrentRegion = ActiveRegion;
	TSet<FName> BlackoutIds;
	TSet<AActor*> Actors;
	if (IsValid(ActiveRegion))
	{
		for (FName Id : ActiveRegion->BlackoutRoomIds)
		{
			if (!Id.IsNone()) BlackoutIds.Add(Id);
		}
		for (AActor* Actor : ActiveRegion->BlackoutActors)
		{
			if (IsValid(Actor)) Actors.Add(Actor);
		}
	}

	for (const TWeakObjectPtr<ATARoomRegion>& Entry : Regions)
	{
		ATARoomRegion* Region = Entry.Get();
		if (!Region) continue;
		Region->bBlackedOut = BlackoutIds.Contains(Region->RoomId);
		if (!Region->bBlackedOut) continue;
		for (AActor* Actor : Region->IncludedAssets)
		{
			if (IsValid(Actor)) Actors.Add(Actor);
		}
	}

	BlackedOutActors.Reset();
	for (AActor* Actor : Actors) BlackedOutActors.Add(TObjectKey<AActor>(Actor));

	// 先构建组件并集。共享 Actor / 组件只保存一次，跨区域仍被引用时保持原始快照。
	TSet<UPrimitiveComponent*> DesiredComponents;
	for (AActor* Actor : Actors)
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (IsValid(Primitive) && Primitive->IsVisible() && !Primitive->bHiddenInGame)
			{
				DesiredComponents.Add(Primitive);
			}
		}
	}

	for (int32 Index = SavedRenderStates.Num() - 1; Index >= 0; --Index)
	{
		const FTARoomSavedRenderState& Saved = SavedRenderStates[Index];
		UPrimitiveComponent* Primitive = Saved.Component.Get();
		if (Primitive && DesiredComponents.Remove(Primitive) > 0) continue;
		if (Primitive)
		{
			Primitive->SetRenderInMainPass(Saved.bRenderInMainPass);
			Primitive->SetRenderInDepthPass(Saved.bRenderInDepthPass);
			Primitive->SetRenderCustomDepth(Saved.bRenderCustomDepth);
			Primitive->SetCustomDepthStencilValue(Saved.CustomDepthStencilValue);
		}
		SavedRenderStates.RemoveAtSwap(Index);
	}

	for (UPrimitiveComponent* Primitive : DesiredComponents)
	{
		FTARoomSavedRenderState& Saved = SavedRenderStates.AddDefaulted_GetRef();
		Saved.Component = Primitive;
		Saved.bRenderInMainPass = Primitive->bRenderInMainPass;
		Saved.bRenderInDepthPass = Primitive->bRenderInDepthPass;
		Saved.bRenderCustomDepth = Primitive->bRenderCustomDepth;
		Saved.CustomDepthStencilValue = Primitive->CustomDepthStencilValue;
		Primitive->SetRenderInMainPass(false);
		Primitive->SetRenderInDepthPass(false);
		Primitive->SetRenderCustomDepth(true);
		Primitive->SetCustomDepthStencilValue(ATARoomRegion::BlackoutStencilValue);
	}
}

bool UTARoomRegistrySubsystem::IsActorInBlackoutRoom(const AActor* Actor)
{
	const UTARoomRegistrySubsystem* Registry = Actor ? Get(Actor) : nullptr;
	if (!Registry) return false;
	if (Registry->BlackedOutActors.Contains(TObjectKey<AActor>(Actor))) return true;

	// 玩家所在区域与被黑场区域的盒子可能在门口重叠，重叠处的物体算玩家这一侧
	const FVector Location = Actor->GetActorLocation();
	const ATARoomRegion* Active = Registry->CurrentRegion.Get();
	if (Active && Active->ContainsPoint(Location)) return false;
	for (const TWeakObjectPtr<ATARoomRegion>& Entry : Registry->Regions)
	{
		const ATARoomRegion* Region = Entry.Get();
		if (Region && Region->IsBlackedOut() && Region->ContainsPoint(Location)) return true;
	}
	return false;
}

void UTARoomRegistrySubsystem::Deinitialize()
{
	ApplyRoomVisibility(nullptr);
	Regions.Reset();
	Super::Deinitialize();
}
