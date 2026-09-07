#include "Movement/TAParkourMarker.h"
#include "Movement/TAParkourComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/TargetPoint.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/TAPromptWidgetUtils.h"
#include "Components/SplineComponent.h"

ATAParkourMarker::ATAParkourMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(80.f, 80.f, 60.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	LandingTargetComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("LandingTargetComponent"));
	LandingTargetComponent->SetupAttachment(RootComponent);
	LandingTargetComponent->SetRelativeLocation(FVector(300.f, 0.f, 0.f));
	LandingTargetComponent->SetChildActorClass(ATargetPoint::StaticClass());

	//落点换成SplineComponent，方便后续做轨迹显示
	LandingSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LandingSpline"));
	LandingSpline->SetupAttachment(RootComponent);
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

	PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidget->SetupAttachment(LandingTargetComponent);
	PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidget->SetDrawAtDesiredSize(true);
	PromptWidget->SetVisibility(false);
}

void ATAParkourMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BindLandingTargetFromChild();
}

void ATAParkourMarker::BeginPlay()
{
	Super::BeginPlay();

	BindLandingTargetFromChild();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATAParkourMarker::OnBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATAParkourMarker::OnEndOverlap);
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

bool ATAParkourMarker::IsValidMarker() const
{
	//return LandingTarget != nullptr;
	return LandingSpline && LandingSpline->GetNumberOfSplinePoints() >= 2;
}
/*
FVector ATAParkourMarker::GetLandingLocation() const
{
	return LandingTarget ? LandingTarget->GetActorLocation() : GetActorLocation();
}
*/

FVector ATAParkourMarker::GetLandingLocation(const FVector& PlayerWorldLocation) const
{
	if (!LandingSpline || LandingSpline->GetNumberOfSplinePoints() < 2)
	{
		return GetActorLocation();
	}

	// 玩家世界坐标转换成 Marker 局部坐标
	const FVector PlayerLocal =
		GetActorTransform().InverseTransformPosition(PlayerWorldLocation);

	const int32 NumPoints = LandingSpline->GetNumberOfSplinePoints();

	const FVector FirstPoint =
		LandingSpline->GetLocationAtSplinePoint(
			0,
			ESplineCoordinateSpace::Local
		);

	const FVector LastPoint =
		LandingSpline->GetLocationAtSplinePoint(
			NumPoints - 1,
			ESplineCoordinateSpace::Local
		);

	// 根据设置决定哪个轴是“前后深度轴”
	const float PlayerDepth =
		bUseXAsDepthAxis ? PlayerLocal.X : PlayerLocal.Y;

	const float FirstDepth =
		bUseXAsDepthAxis ? FirstPoint.X : FirstPoint.Y;

	const float LastDepth =
		bUseXAsDepthAxis ? LastPoint.X : LastPoint.Y;

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
			LandingSpline->GetLocationAtSplineInputKey(
				MidKey,
				ESplineCoordinateSpace::Local
			);

		const float MidDepth =
			bUseXAsDepthAxis
			? MidLocation.X
			: MidLocation.Y;

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

	return LandingSpline->GetLocationAtSplineInputKey(
		FinalKey,
		ESplineCoordinateSpace::World
	);
}

void ATAParkourMarker::RefreshPrompt(UTexture2D* KeyIcon, const FText& PromptText)
{
	if (!PromptWidget)
	{
		return;
	}

	UUserWidget* Widget = PromptWidget->GetUserWidgetObject();
	if (!Widget)
	{
		return;
	}

	FTAPromptWidgetUtils::ApplyPrompt(Widget, KeyIcon, PromptText);
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
		ParkourComp->RegisterMarker(this);
	}
}

void ATAParkourMarker::OnEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	if (UTAParkourComponent* ParkourComp = OtherActor->FindComponentByClass<UTAParkourComponent>())
	{
		ParkourComp->UnregisterMarker(this);
	}
}

void ATAParkourMarker::SetPromptVisible(bool bVisible)
{
	if (PromptWidget)
	{
		PromptWidget->SetVisibility(bVisible);
	}
}