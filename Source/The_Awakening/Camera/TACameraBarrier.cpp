#include "Camera/TACameraBarrier.h"
#include "Camera/TARoomRegion.h"
#include "Camera/CameraTypes.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

FTACameraFrustum FTACameraFrustum::FromAngles(const FVector& Location, const FRotator& Rotation, float TanHalfHorizontal, float TanHalfVertical)
{
	FTACameraFrustum Out;
	if (!(TanHalfHorizontal > 0.f) || !(TanHalfVertical > 0.f) || !FMath::IsFinite(TanHalfHorizontal) || !FMath::IsFinite(TanHalfVertical))
	{
		return Out;
	}
	const FRotationMatrix Axes(Rotation);
	const FVector Forward = Axes.GetScaledAxis(EAxis::X);
	const FVector Right = Axes.GetScaledAxis(EAxis::Y);
	const FVector Up = Axes.GetScaledAxis(EAxis::Z);
	Out.Origin = Location;
	Out.SideNormals[0] = (Right - TanHalfHorizontal * Forward).GetSafeNormal();
	Out.SideNormals[1] = (-Right - TanHalfHorizontal * Forward).GetSafeNormal();
	Out.SideNormals[2] = (Up - TanHalfVertical * Forward).GetSafeNormal();
	Out.SideNormals[3] = (-Up - TanHalfVertical * Forward).GetSafeNormal();
	Out.bValid = true;
	return Out;
}

FTACameraFrustum FTACameraFrustum::FromView(const FMinimalViewInfo& View, const APlayerController* PC)
{
	if (View.ProjectionMode != ECameraProjectionMode::Perspective) return FTACameraFrustum();

	float Aspect = View.AspectRatio;
	EAspectRatioAxisConstraint Constraint = AspectRatio_MaintainXFOV;
	if (!View.bConstrainAspectRatio)
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		if (PC) PC->GetViewportSize(SizeX, SizeY);
		if (SizeX > 0 && SizeY > 0) Aspect = static_cast<float>(SizeX) / static_cast<float>(SizeY);
		if (const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr) Constraint = LocalPlayer->AspectRatioAxisConstraint;
		if (View.AspectRatioAxisConstraint.IsSet()) Constraint = View.AspectRatioAxisConstraint.GetValue();
		if (Constraint == AspectRatio_MajorAxisFOV) Constraint = Aspect >= 1.f ? AspectRatio_MaintainXFOV : AspectRatio_MaintainYFOV;
	}
	if (!(Aspect > KINDA_SMALL_NUMBER)) Aspect = 16.f / 9.f;

	// 与引擎投影矩阵一致：保持水平 FOV 时 FOV 为水平角，保持垂直 FOV 时 FOV 为垂直角
	const float TanBase = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(View.FOV, 1.f, 170.f) * 0.5f));
	const bool bMaintainY = Constraint == AspectRatio_MaintainYFOV;
	const float TanH = bMaintainY ? TanBase * Aspect : TanBase;
	const float TanV = bMaintainY ? TanBase : TanBase / Aspect;
	return FromAngles(View.Location, View.Rotation, TanH, TanV);
}

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
		DirectionArrow->ArrowSize = 2.f;
		DirectionArrow->bIsScreenSizeScaled = true;
		DirectionArrow->SetHiddenInGame(true);
	}
#endif
}

bool ATACameraBarrier::IsActiveForRoom(FName RoomId) const
{
	if (!bBarrierEnabled || !BarrierBox) return false;
	return ActiveInRoomIds.IsEmpty() || (!RoomId.IsNone() && ActiveInRoomIds.Contains(RoomId));
}

FTACameraBarrierInput ATACameraBarrier::MakeInput(float Weight) const
{
	FTACameraBarrierInput Input;
	Input.Weight = Weight;
	if (!BarrierBox) return Input;

	const FTransform& Transform = BarrierBox->GetComponentTransform();
	const FVector Extent = BarrierBox->GetScaledBoxExtent();
	Input.Normal = Transform.GetUnitAxis(EAxis::X);
	const FVector Margin = Input.Normal * SafetyMarginCm;
	int32 Index = 0;
	for (const float Y : {-1.f, 1.f})
	{
		for (const float Z : {-1.f, 1.f})
		{
			// 背向箭头的一面就是“不能看过去”的边界，墙的断面仍然可以出现在画面里
			Input.Corners[Index++] = Transform.TransformPositionNoScale(FVector(-Extent.X, Y * Extent.Y, Z * Extent.Z)) + Margin;
		}
	}
	return Input;
}

float ATACameraBarrier::ComputeRequiredPush(const FTACameraFrustum& Frustum, const FTACameraBarrierInput& Input)
{
	if (!Frustum.bValid) return 0.f;
	float Best = TNumericLimits<float>::Max();
	bool bHasSolution = false;
	for (const FVector& PlaneNormal : Frustum.SideNormals)
	{
		float MinDistance = TNumericLimits<float>::Max();
		for (const FVector& Corner : Input.Corners)
		{
			MinDistance = FMath::Min(MinDistance, static_cast<float>(FVector::DotProduct(Corner - Frustum.Origin, PlaneNormal)));
		}
		// 整个矩形都在这个侧面外：本来就看不到
		if (MinDistance >= 0.f) return 0.f;

		// 相机沿 Normal 移动 s 后距离变为 MinDistance - s * (Normal · PlaneNormal)；只有点积为负的侧面能靠移动分开
		const float Slope = static_cast<float>(FVector::DotProduct(Input.Normal, PlaneNormal));
		if (Slope < -KINDA_SMALL_NUMBER)
		{
			Best = FMath::Min(Best, MinDistance / Slope);
			bHasSolution = true;
		}
	}
	return bHasSolution ? FMath::Max(0.f, Best) : 0.f;
}

FVector ATACameraBarrier::ComputeDisplacement(const FTACameraFrustum& Frustum, TConstArrayView<FTACameraBarrierInput> Inputs)
{
	if (!Frustum.bValid) return FVector::ZeroVector;

	struct FPush { FVector Normal; float Weight; float Distance; };
	TArray<FPush, TInlineAllocator<8>> Pushes;
	for (const FTACameraBarrierInput& Input : Inputs)
	{
		if (Input.Weight <= KINDA_SMALL_NUMBER) continue;
		const float Distance = ComputeRequiredPush(Frustum, Input);
		if (Distance > KINDA_SMALL_NUMBER) Pushes.Add({Input.Normal, FMath::Min(Input.Weight, 1.f), Distance});
	}
	if (Pushes.IsEmpty()) return FVector::ZeroVector;

	// 与第一条推回同轴的挡墙按正 / 反两组各取最强的一面；其他方向的挡墙各自叠加
	const FVector Axis = Pushes[0].Normal;
	const FPush* Positive = nullptr;
	const FPush* Negative = nullptr;
	FVector Other = FVector::ZeroVector;
	for (const FPush& Push : Pushes)
	{
		const double Alignment = FVector::DotProduct(Push.Normal, Axis);
		if (Alignment > 0.99)
		{
			if (!Positive || Push.Weight * Push.Distance > Positive->Weight * Positive->Distance) Positive = &Push;
		}
		else if (Alignment < -0.99)
		{
			if (!Negative || Push.Weight * Push.Distance > Negative->Weight * Negative->Distance) Negative = &Push;
		}
		else
		{
			Other += Push.Normal * (Push.Weight * Push.Distance);
		}
	}

	const float WP = Positive ? Positive->Weight : 0.f;
	const float WN = Negative ? Negative->Weight : 0.f;
	const float DP = Positive ? Positive->Distance : 0.f;
	const float DN = Negative ? Negative->Distance : 0.f;
	float Along = WP * DP - WN * DN;
	// 两侧都要推回：过门交接时随权重平滑过渡；两侧满权重（房间比画面窄）时取中点
	if (Positive && Negative) Along /= FMath::Max(1.f, WP + WN);
	return Axis * Along + Other;
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
