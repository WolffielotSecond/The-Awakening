#include "Camera/TACameraBarrier.h"
#include "Camera/TARoomRegion.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"

ATACameraBarrier::ATACameraBarrier()
{
	PrimaryActorTick.bCanEverTick = false;
	BarrierBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrierBox"));
	BarrierBox->SetBoxExtent(FVector(10.f, 400.f, 250.f));
	BarrierBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierBox->SetGenerateOverlapEvents(false);
	BarrierBox->SetCanEverAffectNavigation(false);
	BarrierBox->ShapeColor = FColor(255, 90, 60);
	BarrierBox->SetHiddenInGame(true);
	RootComponent = BarrierBox;

#if WITH_EDITORONLY_DATA
	DirectionArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	if (DirectionArrow)
	{
		DirectionArrow->SetupAttachment(BarrierBox);
		DirectionArrow->ArrowColor = FColor(255, 90, 60);
		DirectionArrow->ArrowSize = 1.f;
		DirectionArrow->bIsScreenSizeScaled = true;
		DirectionArrow->SetHiddenInGame(true);
	}
#endif
}

FVector ATACameraBarrier::GetBarrierNormal() const
{
	return BarrierBox ? BarrierBox->GetComponentTransform().GetUnitAxis(EAxis::X) : GetActorForwardVector();
}

float ATACameraBarrier::GetLocalThickness(const FVector& WorldPoint) const
{
	if (!BarrierBox) return 0.f;
	return static_cast<float>(BarrierBox->GetComponentTransform().InverseTransformPositionNoScale(WorldPoint).X);
}

float ATACameraBarrier::GetHalfThickness() const
{
	return BarrierBox ? static_cast<float>(BarrierBox->GetScaledBoxExtent().X) : 0.f;
}

int32 ATACameraBarrier::ResolvePlayerSide(const FVector& PlayerLocation, int32 PreviousSide) const
{
	const float Local = GetLocalThickness(PlayerLocation);
	if (PreviousSide == 0) return Local >= 0.f ? 1 : -1;
	// 只有完全走出盒子、到达另一面之外才换边
	const float HalfThickness = GetHalfThickness();
	if (PreviousSide > 0 && Local < -HalfThickness) return -1;
	if (PreviousSide < 0 && Local > HalfThickness) return 1;
	return PreviousSide;
}

float ATACameraBarrier::ComputePush(const FVector& CameraLocation, int32 Side) const
{
	if (!bBarrierEnabled || !BarrierBox || Side == 0) return 0.f;
	const FVector Local = BarrierBox->GetComponentTransform().InverseTransformPositionNoScale(CameraLocation);
	const FVector Extent = BarrierBox->GetScaledBoxExtent();
	// 摄像机在挡墙的宽度 / 高度范围之外：碰不到，不处理
	if (FMath::Abs(Local.Y) > Extent.Y + CameraRadiusCm || FMath::Abs(Local.Z) > Extent.Z + CameraRadiusCm) return 0.f;
	const float Required = static_cast<float>(Extent.X) + CameraRadiusCm;
	return FMath::Max(0.f, Required - static_cast<float>(Side > 0 ? Local.X : -Local.X));
}

FVector ATACameraBarrier::ComputeDisplacement(const FVector& CameraLocation, int32 Side, float CrossAlpha) const
{
	if (!bBarrierEnabled || Side == 0) return FVector::ZeroVector;
	const FVector Normal = GetBarrierNormal() * static_cast<float>(Side > 0 ? 1 : -1);
	const float NewSidePush = ComputePush(CameraLocation, Side);
	const float Alpha = FMath::Clamp(CrossAlpha, 0.f, 1.f);
	if (Alpha >= 1.f) return Normal * NewSidePush;

	// 换边过渡：在“挡在旧一侧”与“挡在新一侧”之间插值，摄像机平滑穿过挡墙
	const float OldSidePush = ComputePush(CameraLocation, -Side);
	return Normal * (Alpha * NewSidePush) - Normal * ((1.f - Alpha) * OldSidePush);
}

void ATACameraBarrier::BeginPlay()
{
	Super::BeginPlay();
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->RegisterBarrier(this);
}

void ATACameraBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this)) Registry->UnregisterBarrier(this);
	Super::EndPlay(EndPlayReason);
}
