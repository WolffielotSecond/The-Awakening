#include "Camera/TARoomViewComponent.h"
#include "Camera/TARoomRegion.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/CapsuleComponent.h"
#include "Core/TAFreezeExemptSpringArm.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

namespace
{
	/** 原 DoLook 的软限位：越接近边界，向外推的增量衰减越强；向内回收不受限。 */
	float ApplyStrongSoftLimit(float Current, float Delta, float Limit)
	{
		const float SoftStartRatio = 0.2f;
		if (Limit <= KINDA_SMALL_NUMBER || FMath::IsNearlyZero(Delta)) return 0.f;
		if ((Current * Delta) <= 0.f) return Delta;

		const float AbsCurrent = FMath::Abs(Current);
		const float Ratio = AbsCurrent / Limit;
		if (Ratio < SoftStartRatio && FMath::Abs(Current + Delta) <= Limit * SoftStartRatio) return Delta;

		const float Remaining = FMath::Max(0.f, Limit - AbsCurrent);
		float T = 0.f;
		if (Ratio >= SoftStartRatio)
		{
			T = FMath::Clamp((Ratio - SoftStartRatio) / FMath::Max(1.f - SoftStartRatio, KINDA_SMALL_NUMBER), 0.f, 1.f);
		}
		const float Ease = 1.f - (T * T * T);
		const float MaxStep = Remaining * FMath::Lerp(0.35f, 0.03f, T);
		float ClampedDelta = Delta * Ease;
		if (FMath::Abs(ClampedDelta) > MaxStep) ClampedDelta = FMath::Sign(ClampedDelta) * MaxStep;
		return ClampedDelta;
	}

	FTARoomCameraSettings LerpSettings(const FTARoomCameraSettings& A, const FTARoomCameraSettings& B, float Alpha)
	{
		FTARoomCameraSettings Out = B;
		Out.ArmLengthCm = FMath::Lerp(A.ArmLengthCm, B.ArmLengthCm, Alpha);
		Out.HeightOffsetCm = FMath::Lerp(A.HeightOffsetCm, B.HeightOffsetCm, Alpha);
		Out.LookLeftCm = FMath::Lerp(A.LookLeftCm, B.LookLeftCm, Alpha);
		Out.LookRightCm = FMath::Lerp(A.LookRightCm, B.LookRightCm, Alpha);
		Out.LookUpCm = FMath::Lerp(A.LookUpCm, B.LookUpCm, Alpha);
		Out.LookDownCm = FMath::Lerp(A.LookDownCm, B.LookDownCm, Alpha);
		return Out;
	}
}

UTARoomViewComponent::UTARoomViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	// 扫描冻结会停掉角色上非豁免组件的 Tick，镜头构图不能跟着停
	ComponentTags.AddUnique(TEXT("FreezeExempt"));
	BlackoutPostProcessMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Camera/RoomView/Materials/MI_RoomBlackoutPP.MI_RoomBlackoutPP")));
}

void UTARoomViewComponent::Initialize(USpringArmComponent* InBoom, UCameraComponent* InCamera, const FTARoomCameraSettings& InFallback)
{
	if (Boom.IsValid()) Boom->RemoveTickPrerequisiteComponent(this);
	Boom = InBoom;
	FallbackSettings = InFallback;
	if (InCamera && InCamera != Camera.Get())
	{
		if (UMaterialInterface* Blackout = BlackoutPostProcessMaterial.LoadSynchronous())
		{
			InCamera->PostProcessSettings.AddBlendable(Blackout, 1.f);
		}
	}
	Camera = InCamera;
	// 保证本组件先写参数，Boom 同帧再求解，避免差一帧
	if (InBoom) InBoom->AddTickPrerequisiteComponent(this);
	bInitialized = true;
	SnapToCurrentLocation();
}

FVector UTARoomViewComponent::GetProbeLocation() const
{
	// 用胶囊底部判断所在区域，不用相机位置：只移动镜头不能改变所在房间
	const AActor* Owner = GetOwner();
	if (!Owner) return FVector::ZeroVector;
	FVector Probe = Owner->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Probe.Z -= Capsule->GetScaledCapsuleHalfHeight() - 5.f;
		}
	}
	return Probe;
}

void UTARoomViewComponent::ResolveRegion(bool bSnap)
{
	const UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this);
	ATARoomRegion* Region = Registry ? Registry->ResolveRegion(GetProbeLocation(), ActiveRegion.Get()) : nullptr;

	const bool bRegionChanged = Region != ActiveRegion.Get() || (!Region && ActiveRoomId != NAME_None);
	if (!bRegionChanged && !bSnap) return;

	ActiveRegion = Region;
	ActiveRoomId = Region ? Region->RoomId : NAME_None;
	const FTARoomCameraSettings Target = (Region && Region->CameraProfile) ? Region->CameraProfile->Settings : FallbackSettings;

	BlendFrom = CurrentSettings;
	BlendTo = Target;
	BlendElapsed = 0.f;
	BlendDuration = bSnap ? 0.f : Target.BlendSeconds;
	if (BlendDuration <= 0.f) CurrentSettings = Target;
}

void UTARoomViewComponent::UpdateRoomVisibility()
{
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this))
	{
		Registry->ApplyRoomVisibility(ActiveRegion.Get());
	}
}

void UTARoomViewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTARoomRegistrySubsystem* Registry = UTARoomRegistrySubsystem::Get(this))
	{
		Registry->ApplyRoomVisibility(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void UTARoomViewComponent::SnapToCurrentLocation()
{
	if (!bInitialized) return;
	LookOffset = FVector2D::ZeroVector;
	ResolveRegion(true);
	UpdateRoomVisibility();
	bSnapOnNextTick = true;
	ApplyToBoom();
	if (auto* ExemptBoom = Cast<UTAFreezeExemptSpringArm>(Boom.Get())) ExemptBoom->SkipLagOnce();
}

void UTARoomViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// 与 Boom 一致使用世界时间，不受角色 CustomTimeDilation（扫描冻结）影响
	const UWorld* World = GetWorld();
	const float WorldDelta = World ? World->GetDeltaSeconds() : DeltaTime;
	Super::TickComponent(WorldDelta, TickType, ThisTickFunction);
	if (!bInitialized) return;

	// 初始化 / 复活后的下一帧再按位置直接到位一次：区域可能比角色晚 BeginPlay，避免开局从默认镜头缓慢拉到房间镜头
	ResolveRegion(bSnapOnNextTick);
	bSnapOnNextTick = false;
	UpdateRoomVisibility();

	if (BlendDuration > 0.f && BlendElapsed < BlendDuration)
	{
		BlendElapsed = FMath::Min(BlendElapsed + WorldDelta, BlendDuration);
		const float Alpha = FMath::SmoothStep(0.f, 1.f, BlendElapsed / BlendDuration);
		CurrentSettings = LerpSettings(BlendFrom, BlendTo, Alpha);
	}

	// 范围变小时偏移跟着限位一起平滑收回，不会把大房间的偏移带进小房间
	ClampLookOffsetToCurrentLimits();
	ApplyToBoom();
}

void UTARoomViewComponent::AddLookInput(float WorldYCm, float WorldZCm)
{
	// 把输入换算到屏幕空间再套左右限位
	const float DeltaH = WorldYCm * GetWorldYScreenSign();
	const float DeltaV = WorldZCm;
	// 按当前偏移所在一侧取限位（在中心时按输入方向），向内回收不会被另一侧的 0 限位卡住
	const bool bRightSide = LookOffset.X > 0.f || (LookOffset.X == 0.f && DeltaH > 0.f);
	const bool bUpSide = LookOffset.Y > 0.f || (LookOffset.Y == 0.f && DeltaV > 0.f);
	const float LimitH = bRightSide ? CurrentSettings.LookRightCm : CurrentSettings.LookLeftCm;
	const float LimitV = bUpSide ? CurrentSettings.LookUpCm : CurrentSettings.LookDownCm;

	LookOffset.X += ApplyStrongSoftLimit(LookOffset.X, DeltaH, LimitH);
	LookOffset.Y += ApplyStrongSoftLimit(LookOffset.Y, DeltaV, LimitV);
	ClampLookOffsetToCurrentLimits();
	ApplyToBoom();
}

float UTARoomViewComponent::GetWorldYScreenSign() const
{
	// 世界 +Y 在屏幕右侧为 1，左侧为 -1；相机朝向由蓝图决定，不在这里假设
	const UCameraComponent* Cam = Camera.Get();
	return (Cam && FVector::DotProduct(Cam->GetRightVector(), FVector::YAxisVector) < -KINDA_SMALL_NUMBER) ? -1.f : 1.f;
}

void UTARoomViewComponent::ClampLookOffsetToCurrentLimits()
{
	LookOffset.X = FMath::Clamp(LookOffset.X, -CurrentSettings.LookLeftCm, CurrentSettings.LookRightCm);
	LookOffset.Y = FMath::Clamp(LookOffset.Y, -CurrentSettings.LookDownCm, CurrentSettings.LookUpCm);
}

void UTARoomViewComponent::ApplyToBoom()
{
	USpringArmComponent* Arm = Boom.Get();
	if (!Arm) return;
	Arm->TargetArmLength = CurrentSettings.ArmLengthCm;
	Arm->SocketOffset = FVector(0.f, 0.f, CurrentSettings.HeightOffsetCm);
	Arm->TargetOffset = FVector(0.f, LookOffset.X * GetWorldYScreenSign(), LookOffset.Y);
}
