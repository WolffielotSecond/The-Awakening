#include "Movement/TAParkourMarker.h"
#include "Movement/TAParkourComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Engine/TargetPoint.h"
#include "Components/SplineComponent.h"
#include "Serialization/CustomVersion.h"
namespace
{
	const FGuid ParkourLandingVersion(0x8F45251A, 0xC69B4ED2, 0xB9717D60, 0xDA3C0716);
	FCustomVersionRegistration RegisterParkourLandingVersion(ParkourLandingVersion, 1, TEXT("DirectionalParkourLandings"));
}
#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#endif

ATAParkourMarker::ATAParkourMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(SceneRoot);

	TriggerBox->SetBoxExtent(FVector(80.f, 80.f, 60.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	LandingTargetComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("LandingTargetComponent"));
	LandingTargetComponent->SetupAttachment(SceneRoot);
	LandingTargetComponent->SetRelativeLocation(FVector(300.f, 0.f, 0.f));
	LandingTargetComponent->SetChildActorClass(ATargetPoint::StaticClass());

	// Landing splines define depth-matched endpoints, not the flight path.
	LandingSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LandingSpline"));
	LandingSpline->SetupAttachment(SceneRoot);
	LandingSpline->ClearSplinePoints(false);

	LandingSpline->AddSplinePoint(
		FVector(-100.f, 300.f, 0.f),
		ESplineCoordinateSpace::Local,
		false
	);

	LandingSpline->AddSplinePoint(
		FVector(100.f, 300.f, 0.f),
		ESplineCoordinateSpace::Local,
		false
	);

	LandingSpline->UpdateSpline();

	// 保留原组件名，BP_ParkourMarker 对它的 Widget Class 设置继续生效
	PromptWidget = CreateDefaultSubobject<UTAInteractionPromptAnchor>(TEXT("PromptWidget"));
	PromptWidget->SetupAttachment(LandingTargetComponent);
	// 原先显示时被提示组件改写为 Z=-50，这里直接用这个实际显示位置
	PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
	PromptWidget->PromptType = ETAPromptType::ParkourJump;
	AdditionalLandingSpline = CreateDefaultSubobject<USplineComponent>(TEXT("AdditionalLandingSpline"));
	AdditionalLandingSpline->SetupAttachment(SceneRoot);
	AdditionalLandingSpline->ClearSplinePoints(false);
	AdditionalLandingSpline->AddSplinePoint(FVector(-100, 300, -200), ESplineCoordinateSpace::Local, false);
	AdditionalLandingSpline->AddSplinePoint(FVector(100, 300, -200), ESplineCoordinateSpace::Local, true);
	AdditionalLandingTargetComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("AdditionalLandingTargetComponent"));
	AdditionalLandingTargetComponent->SetupAttachment(SceneRoot);
	AdditionalLandingTargetComponent->SetChildActorClass(ATargetPoint::StaticClass());
	AdditionalPromptWidget = CreateDefaultSubobject<UTAInteractionPromptAnchor>(TEXT("AdditionalPromptWidget"));
	AdditionalPromptWidget->SetupAttachment(AdditionalLandingTargetComponent);
	AdditionalPromptWidget->SetRelativeLocation(FVector(0, 0, -50));

}

void ATAParkourMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitializeLandingConfiguration();
	BindLandingTargetFromChild();
	SyncPromptType();
	HideLandingAnchor();
}

void ATAParkourMarker::BeginPlay()
{
	Super::BeginPlay();
	InitializeLandingConfiguration();

	BindLandingTargetFromChild();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATAParkourMarker::OnBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATAParkourMarker::OnEndOverlap);
	SyncPromptType();
	HideLandingAnchor();
}

void ATAParkourMarker::BindLandingTargetFromChild()
{
	if (LandingTargetComponent)
	{
		if (AActor* Child = LandingTargetComponent->GetChildActor())
		{
			LandingTarget = Child;
		}
	}
}

void ATAParkourMarker::PostLoad()
{
	Super::PostLoad();
	// Per-object archive version handles old instance MarkerType overrides even when a BP CDO migrated first.
	if (bLoadedLegacyLanding) bDirectionalLandingConfigured = false;
	InitializeLandingConfiguration();
}

void ATAParkourMarker::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(ParkourLandingVersion);
	if (Ar.IsLoading()) bLoadedLegacyLanding = Ar.CustomVer(ParkourLandingVersion) < 1;
}

void ATAParkourMarker::InitializeLandingConfiguration()
{
	if (!bDirectionalLandingConfigured)
	{
		bLegacySplineIsLower = MarkerType == ETAParkourMarkerType::DropDown;
		bEnableUpper = !bLegacySplineIsLower;
		bEnableLower = bLegacySplineIsLower;
		if (bLegacySplineIsLower) { LowerArcHeight = ArcHeight; LowerJumpDuration = JumpDuration; }
		else { UpperArcHeight = ArcHeight; UpperJumpDuration = JumpDuration; }
		bDirectionalLandingConfigured = true;
	}
	// Visibility never edits SplineCurves. Selection visualization may still be drawn by UE.
	for (bool bLower : {false, true})
	{
		if (auto* Spline = GetLandingSpline(bLower))
		{
			Spline->SetVisibility(IsLandingEnabled(bLower));
			Spline->bDrawDebug = IsLandingEnabled(bLower);
			Spline->SetHiddenInGame(true);
		}
		if (!IsLandingEnabled(bLower))
			if (auto* Anchor = GetLandingPrompt(bLower)) Anchor->HidePrompt();
	}
}

USplineComponent* ATAParkourMarker::GetLandingSpline(bool bLower) const
{ return bLower == bLegacySplineIsLower ? LandingSpline.Get() : AdditionalLandingSpline.Get(); }
UChildActorComponent* ATAParkourMarker::GetLandingTargetComponent(bool bLower) const
{ return bLower == bLegacySplineIsLower ? LandingTargetComponent.Get() : AdditionalLandingTargetComponent.Get(); }
UTAInteractionPromptAnchor* ATAParkourMarker::GetLandingPrompt(bool bLower) const
{ return bLower == bLegacySplineIsLower ? PromptWidget.Get() : AdditionalPromptWidget.Get(); }
bool ATAParkourMarker::HasValidLanding(bool bLower) const
{
	const auto* Spline = GetLandingSpline(bLower);
	return IsLandingEnabled(bLower) && Spline && Spline->GetNumberOfSplinePoints() >= 2;
}
bool ATAParkourMarker::IsValidMarker() const
{
	// An enabled malformed slot invalidates the marker, rather than silently changing selection.
	return (bEnableUpper || bEnableLower) && (!bEnableUpper || HasValidLanding(false)) &&
		(!bEnableLower || HasValidLanding(true));
}
void ATAParkourMarker::UpdateLandingTargets(const FVector& PlayerLocation)
{
	for (bool bLower : {false, true})
		if (HasValidLanding(bLower))
			if (auto* Target = GetLandingTargetComponent(bLower))
				Target->SetWorldLocation(GetLandingLocationForSlot(PlayerLocation, bLower));
}

#if WITH_EDITOR
void ATAParkourMarker::CheckForErrors()
{
	Super::CheckForErrors();
	auto Warn = [&](const FText& Text)
	{
		FMessageLog("MapCheck").Warning()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(Text));
	};
	if (!IsValidMarker()) Warn(FText::FromString(TEXT("Parkour: enable at least one landing; enabled splines need two points.")));
	for (bool bLower : {false, true})
	{
		if (!HasValidLanding(bLower)) continue;
		const auto* Spline = GetLandingSpline(bLower);
		float Previous = 0; int32 Direction = 0;
		for (int32 I = 0; I <= 64; ++I)
		{
			const FVector World = Spline->GetLocationAtSplineInputKey((Spline->GetNumberOfSplinePoints() - 1) * I / 64.f, ESplineCoordinateSpace::World);
			const FVector Local = GetActorTransform().InverseTransformPosition(World);
			const float Depth = bUseXAsDepthAxis ? Local.X : Local.Y;
			if (I && !FMath::IsNearlyEqual(Depth, Previous))
			{
				const int32 Sign = Depth > Previous ? 1 : -1;
				if (Direction && Sign != Direction) { Warn(FText::FromString(TEXT("Parkour: spline depth is not monotonic; binary depth search requires a monotonic spline."))); break; }
				Direction = Sign;
			}
			Previous = Depth;
		}
	}
}
#endif

FVector ATAParkourMarker::GetLandingLocation(const FVector& PlayerWorldLocation) const
{ return GetLandingLocationForSlot(PlayerWorldLocation, !bEnableUpper); }

FVector ATAParkourMarker::GetLandingLocationForSlot(const FVector& PlayerWorldLocation, bool bLower) const
{
	const USplineComponent* Spline = GetLandingSpline(bLower);
	if (!HasValidLanding(bLower) || Spline->GetNumberOfSplinePoints() < 2)
	{
		return GetActorLocation();
	}

	// 玩家世界坐标转换成 Marker 局部坐标
	const FVector PlayerLocal =
		GetActorTransform().InverseTransformPosition(PlayerWorldLocation);

	const int32 NumPoints = Spline->GetNumberOfSplinePoints();

	const FVector FirstPoint =
		Spline->GetLocationAtSplinePoint(
			0,
			ESplineCoordinateSpace::World
		);

	const FVector LastPoint =
		Spline->GetLocationAtSplinePoint(
			NumPoints - 1,
			ESplineCoordinateSpace::World
		);

	// 根据设置决定哪个轴是“前后深度轴”
	const float PlayerDepth =
		bUseXAsDepthAxis ? PlayerLocal.X : PlayerLocal.Y;

	const float FirstDepth =
		bUseXAsDepthAxis ? GetActorTransform().InverseTransformPosition(FirstPoint).X : GetActorTransform().InverseTransformPosition(FirstPoint).Y;

	const float LastDepth =
		bUseXAsDepthAxis ? GetActorTransform().InverseTransformPosition(LastPoint).X : GetActorTransform().InverseTransformPosition(LastPoint).Y;

	const float MinDepth = FMath::Min(FirstDepth, LastDepth);
	const float MaxDepth = FMath::Max(FirstDepth, LastDepth);

	// 玩家深度限制在 LandingSpline 覆盖范围内
	const float TargetDepth =
		FMath::Clamp(PlayerDepth, MinDepth, MaxDepth);

	float LowKey = 0.f;
	float HighKey = static_cast<float>(NumPoints - 1);

	const bool bIncreasingDepth =
		LastDepth >= FirstDepth;

	// 根据深度轴反查 Spline 上对应位置
	for (int32 i = 0; i < 20; ++i)
	{
		const float MidKey =
			(LowKey + HighKey) * 0.5f;

		const FVector MidLocation =
			Spline->GetLocationAtSplineInputKey(
				MidKey,
				ESplineCoordinateSpace::World
			);

		const float MidDepth =
			bUseXAsDepthAxis
			? GetActorTransform().InverseTransformPosition(MidLocation).X
			: GetActorTransform().InverseTransformPosition(MidLocation).Y;

		if (bIncreasingDepth)
		{
			if (MidDepth < TargetDepth)
			{
				LowKey = MidKey;
			}
			else
			{
				HighKey = MidKey;
			}
		}
		else
		{
			if (MidDepth > TargetDepth)
			{
				LowKey = MidKey;
			}
			else
			{
				HighKey = MidKey;
			}
		}
	}

	const float FinalKey =
		(LowKey + HighKey) * 0.5f;

	return Spline->GetLocationAtSplineInputKey(
		FinalKey,
		ESplineCoordinateSpace::World
	);
}

void ATAParkourMarker::OnBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	if (UTAParkourComponent* ParkourComp = OtherActor->FindComponentByClass<UTAParkourComponent>())
	{
		CurrentOverlappingActor = OtherActor;
		SetActorTickEnabled(true);
		ParkourComp->RegisterMarker(this);
	}
}

void ATAParkourMarker::OnEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	// UE removes the ending pair before notifying; another component still overlapping is not actor exit.
	if (!OtherActor || (TriggerBox && TriggerBox->IsOverlappingActor(OtherActor)))
	{
		return;
	}

	if (UTAParkourComponent* ParkourComp = OtherActor->FindComponentByClass<UTAParkourComponent>())
	{
		ParkourComp->UnregisterMarker(this);
		if (CurrentOverlappingActor.Get() == OtherActor)
		{
			CurrentOverlappingActor.Reset();
			SetActorTickEnabled(false);
		}
	}
}

void ATAParkourMarker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CurrentOverlappingActor.IsValid() || !LandingTargetComponent)
	{
		return;
	}

	UpdateLandingTargets(CurrentOverlappingActor->GetActorLocation());
}
