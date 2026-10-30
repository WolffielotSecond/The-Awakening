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

void ATARoomRegion::SetBlackedOut(bool bInBlackedOut)
{
	if (bBlackedOut == bInBlackedOut) return;
	bBlackedOut = bInBlackedOut;

	if (!bBlackedOut)
	{
		RestoreRenderState();
		return;
	}

	// 不进主渲染、只写 CustomDepth：走廊等可见物体全部盖在上面，后处理只在空白处画出暗色轮廓。
	// 不隐藏 Actor、不关碰撞，交互 / 剧情 / 声音照常运行
	for (const AActor* Actor : DetailActors)
	{
		if (!Actor) continue;
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive || !Primitive->IsVisible() || Primitive->bHiddenInGame) continue;
			FTARoomSavedRenderState& Saved = SavedRenderStates.AddDefaulted_GetRef();
			Saved.Component = Primitive;
			Saved.bRenderInMainPass = Primitive->bRenderInMainPass;
			Saved.bRenderInDepthPass = Primitive->bRenderInDepthPass;
			Saved.bRenderCustomDepth = Primitive->bRenderCustomDepth;
			Saved.CustomDepthStencilValue = Primitive->CustomDepthStencilValue;

			Primitive->SetRenderInMainPass(false);
			Primitive->SetRenderInDepthPass(false);
			Primitive->SetRenderCustomDepth(true);
			Primitive->SetCustomDepthStencilValue(BlackoutStencilValue);
		}
	}
}

void ATARoomRegion::RestoreRenderState()
{
	for (const FTARoomSavedRenderState& Saved : SavedRenderStates)
	{
		UPrimitiveComponent* Primitive = Saved.Component.Get();
		if (!Primitive) continue;
		Primitive->SetRenderInMainPass(Saved.bRenderInMainPass);
		Primitive->SetRenderInDepthPass(Saved.bRenderInDepthPass);
		Primitive->SetRenderCustomDepth(Saved.bRenderCustomDepth);
		Primitive->SetCustomDepthStencilValue(Saved.CustomDepthStencilValue);
	}
	SavedRenderStates.Reset();
}

void ATARoomRegion::BeginPlay()
{
	Super::BeginPlay();
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->RegisterRegion(this);
}

void ATARoomRegion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->UnregisterRegion(this);
	SetBlackedOut(false);
	Super::EndPlay(EndPlayReason);
}

ATARoomPortal::ATARoomPortal()
{
	PrimaryActorTick.bCanEverTick = false;
	PortalBox = CreateTriggerBox(this, TEXT("PortalBox"), FVector(60.f, 60.f, 120.f), FColor(255, 200, 60));
	RootComponent = PortalBox;
}

bool ATARoomPortal::ContainsPoint(const FVector& WorldPoint, bool bUseExitMargin) const
{
	return BoxContainsPoint(PortalBox, WorldPoint, bUseExitMargin ? ExitMarginCm : 0.f);
}

void ATARoomPortal::BeginPlay()
{
	Super::BeginPlay();
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->RegisterPortal(this);
}

void ATARoomPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->UnregisterPortal(this);
	Super::EndPlay(EndPlayReason);
}

UTARoomRegistrySubsystem* UTARoomRegistrySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTARoomRegistrySubsystem>() : nullptr;
}

void UTARoomRegistrySubsystem::RegisterRegion(ATARoomRegion* Region)
{
	if (Region) Regions.AddUnique(Region);
}

void UTARoomRegistrySubsystem::UnregisterRegion(ATARoomRegion* Region)
{
	Regions.RemoveAll([Region](const TWeakObjectPtr<ATARoomRegion>& Entry) { return !Entry.IsValid() || Entry.Get() == Region; });
}

void UTARoomRegistrySubsystem::RegisterPortal(ATARoomPortal* Portal)
{
	if (Portal) Portals.AddUnique(Portal);
}

void UTARoomRegistrySubsystem::UnregisterPortal(ATARoomPortal* Portal)
{
	Portals.RemoveAll([Portal](const TWeakObjectPtr<ATARoomPortal>& Entry) { return !Entry.IsValid() || Entry.Get() == Portal; });
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

void UTARoomRegistrySubsystem::ResolvePortals(const FVector& WorldPoint, FName ActiveRoomId, TArray<TWeakObjectPtr<ATARoomPortal>>& InOutActivePortals) const
{
	TArray<TWeakObjectPtr<ATARoomPortal>> Next;
	for (const TWeakObjectPtr<ATARoomPortal>& Entry : Portals)
	{
		const ATARoomPortal* Portal = Entry.Get();
		if (!Portal || !Portal->Connects(ActiveRoomId)) continue;
		if (Portal->ContainsPoint(WorldPoint, InOutActivePortals.Contains(Entry))) Next.Add(Entry);
	}
	InOutActivePortals = MoveTemp(Next);
}

void UTARoomRegistrySubsystem::ApplyRoomVisibility(const TSet<FName>& VisibleRooms, bool bShowAll) const
{
	for (const TWeakObjectPtr<ATARoomRegion>& Entry : Regions)
	{
		if (ATARoomRegion* Region = Entry.Get())
		{
			Region->SetBlackedOut(!bShowAll && !VisibleRooms.Contains(Region->RoomId));
		}
	}
}
