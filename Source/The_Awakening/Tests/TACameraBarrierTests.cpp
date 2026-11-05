#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Camera/TACameraBarrier.h"
#include "Camera/TARoomRegion.h"
#include "Camera/TARoomViewComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace TACameraBarrierTests
{
	/** 镜头位于原点朝 +X，水平半角 45°（tan = 1），垂直 tan = 0.5。 */
	FTACameraFrustum FrustumAt(const FVector& Location)
	{
		return FTACameraFrustum::FromAngles(Location, FRotator::ZeroRotator, 1.f, 0.5f);
	}

	/** 墙面在 Y = PlaneY，房间深度 X 100–1000，高度 ±200；Normal 指向允许一侧。 */
	FTACameraBarrierInput Wall(float PlaneY, const FVector& Normal, float Weight)
	{
		FTACameraBarrierInput Input;
		Input.Normal = Normal;
		Input.Weight = Weight;
		int32 Index = 0;
		for (const float X : {100.f, 1000.f})
		{
			for (const float Z : {-200.f, 200.f}) Input.Corners[Index++] = FVector(X, PlaneY, Z);
		}
		return Input;
	}

	struct FWorldFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		UTARoomRegistrySubsystem* Registry = nullptr;

		FWorldFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Registry = World->GetSubsystem<UTARoomRegistrySubsystem>();
		}
		~FWorldFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		/** 箭头朝 -Y，背向箭头的一面落在 Y = 500，X 100–1000，Z ±200。 */
		ATACameraBarrier* Barrier()
		{
			ATACameraBarrier* B = World->SpawnActor<ATACameraBarrier>(FVector(550.f, 490.f, 0.f), FRotator(0.f, -90.f, 0.f));
			B->GetBarrierBox()->SetBoxExtent(FVector(10.f, 450.f, 200.f));
			B->SafetyMarginCm = 0.f;
			Registry->RegisterBarrier(B);
			return B;
		}

		ATARoomRegion* Region(FName Id, const FVector& Location)
		{
			ATARoomRegion* R = World->SpawnActor<ATARoomRegion>(Location, FRotator::ZeroRotator);
			R->RoomId = Id;
			Registry->RegisterRegion(R);
			return R;
		}
	};

	FMinimalViewInfo ViewAt(const FVector& Location)
	{
		FMinimalViewInfo View;
		View.Location = Location;
		View.Rotation = FRotator::ZeroRotator;
		View.FOV = 90.f;
		View.AspectRatio = 2.f;
		View.bConstrainAspectRatio = true;
		return View;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierPushTest, "TheAwakening.Camera.Barrier.FrustumEdgePush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierPushTest::RunTest(const FString&)
{
	using namespace TACameraBarrierTests;
	const FTACameraBarrierInput Right = Wall(500.f, FVector(0.f, -1.f, 0.f), 1.f);
	TestEqual(TEXT("Frustum edge at the deepest point is pushed exactly to the wall"),
		ATACameraBarrier::ComputeRequiredPush(FrustumAt(FVector::ZeroVector), Right), 500.f, 0.01f);
	TestEqual(TEXT("Already clear of the wall needs no push"),
		ATACameraBarrier::ComputeRequiredPush(FrustumAt(FVector(0.f, -600.f, 0.f)), Right), 0.f, 0.001f);
	TestEqual(TEXT("Camera that already passed the wall is pulled back to the protected side"),
		ATACameraBarrier::ComputeRequiredPush(FrustumAt(FVector(0.f, 700.f, 0.f)), Right), 1200.f, 0.01f);

	const FTACameraBarrierInput Inputs[] = {Right};
	TestTrue(TEXT("Full weight displacement"), ATACameraBarrier::ComputeDisplacement(FrustumAt(FVector::ZeroVector), Inputs).Equals(FVector(0.f, -500.f, 0.f), 0.01f));
	const FTACameraBarrierInput Half[] = {Wall(500.f, FVector(0.f, -1.f, 0.f), 0.5f)};
	TestTrue(TEXT("Partial weight scales the push"), ATACameraBarrier::ComputeDisplacement(FrustumAt(FVector::ZeroVector), Half).Equals(FVector(0.f, -250.f, 0.f), 0.01f));

	const FTACameraFrustum Wide = FTACameraFrustum::FromAngles(FVector::ZeroVector, FRotator::ZeroRotator, 1.5f, 0.5f);
	TestEqual(TEXT("Wider aspect needs a larger push"), ATACameraBarrier::ComputeRequiredPush(Wide, Right), 1000.f, 0.01f);

	FMinimalViewInfo View = ViewAt(FVector::ZeroVector);
	const FTACameraFrustum FromView = FTACameraFrustum::FromView(View, nullptr);
	TestEqual(TEXT("Frustum from view uses horizontal FOV"), ATACameraBarrier::ComputeRequiredPush(FromView, Right), 500.f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierPairTest, "TheAwakening.Camera.Barrier.OppositeWallsAndHandover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierPairTest::RunTest(const FString&)
{
	using namespace TACameraBarrierTests;
	const FTACameraFrustum Frustum = FrustumAt(FVector::ZeroVector);
	const FTACameraBarrierInput Narrow[] = {Wall(500.f, FVector(0.f, -1.f, 0.f), 1.f), Wall(-500.f, FVector(0.f, 1.f, 0.f), 1.f)};
	TestTrue(TEXT("Room narrower than the view uses the midpoint and stays put"),
		ATACameraBarrier::ComputeDisplacement(Frustum, Narrow).Equals(FVector::ZeroVector, 0.01f));

	float Previous = -TNumericLimits<float>::Max();
	bool bMonotonic = true;
	for (int32 Step = 0; Step <= 10; ++Step)
	{
		const float Alpha = Step / 10.f;
		const FTACameraBarrierInput Handover[] = {Wall(500.f, FVector(0.f, -1.f, 0.f), 1.f - Alpha), Wall(-500.f, FVector(0.f, 1.f, 0.f), Alpha)};
		const float Y = static_cast<float>(ATACameraBarrier::ComputeDisplacement(Frustum, Handover).Y);
		const float Expected = -500.f * (1.f - Alpha) + 500.f * Alpha;
		TestEqual(*FString::Printf(TEXT("Handover at %.1f blends linearly between both framings"), Alpha), Y, Expected, 0.01f);
		if (Previous > Y + 0.01f) bMonotonic = false;
		Previous = Y;
	}
	TestTrue(TEXT("Handover never jumps back"), bMonotonic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierActorTest, "TheAwakening.Camera.Barrier.ActorAndRoomRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierActorTest::RunTest(const FString&)
{
	using namespace TACameraBarrierTests;
	FWorldFixture F;
	ATACameraBarrier* Barrier = F.Barrier();
	const FTACameraBarrierInput Input = Barrier->MakeInput(1.f);
	TestTrue(TEXT("Arrow is the protected direction"), Input.Normal.Equals(FVector(0.f, -1.f, 0.f), 0.001f));
	TestEqual(TEXT("Face away from the arrow is the boundary"), ATACameraBarrier::ComputeRequiredPush(FrustumAt(FVector::ZeroVector), Input), 500.f, 0.05f);
	Barrier->SafetyMarginCm = 10.f;
	TestEqual(TEXT("Safety margin keeps the edge further from the wall"),
		ATACameraBarrier::ComputeRequiredPush(FrustumAt(FVector::ZeroVector), Barrier->MakeInput(1.f)), 510.f, 0.05f);

	TestTrue(TEXT("Empty room list is always active"), Barrier->IsActiveForRoom(NAME_None));
	Barrier->ActiveInRoomIds = {TEXT("Home.Living")};
	TestTrue(TEXT("Active in its own room"), Barrier->IsActiveForRoom(TEXT("Home.Living")));
	TestFalse(TEXT("Inactive in the next room"), Barrier->IsActiveForRoom(TEXT("Building.Corridor")));
	TestFalse(TEXT("Inactive outside all regions"), Barrier->IsActiveForRoom(NAME_None));
	Barrier->bBarrierEnabled = false;
	TestFalse(TEXT("Disabled barrier never applies"), Barrier->IsActiveForRoom(TEXT("Home.Living")));
	F.Registry->UnregisterBarrier(Barrier);
	TestEqual(TEXT("Unregistered barrier is removed"), F.Registry->GetBarriers().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierRoomViewTest, "TheAwakening.Camera.Barrier.RoomViewWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierRoomViewTest::RunTest(const FString&)
{
	using namespace TACameraBarrierTests;
	FWorldFixture F;
	F.Region(TEXT("Home.Living"), FVector(0.f, 0.f, 0.f));
	F.Region(TEXT("Building.Corridor"), FVector(0.f, 3000.f, 0.f));
	ATACameraBarrier* Barrier = F.Barrier();
	Barrier->ActiveInRoomIds = {TEXT("Home.Living")};

	AActor* Owner = F.World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	UTARoomViewComponent* RoomView = NewObject<UTARoomViewComponent>(Owner);
	RoomView->RegisterComponent();
	RoomView->Initialize(nullptr, nullptr, FTARoomCameraSettings());
	TestTrue(TEXT("Starts in the protected room"), RoomView->GetActiveRoomId() == FName(TEXT("Home.Living")));
	TestEqual(TEXT("Barrier is fully active on start"), RoomView->GetBarrierWeight(Barrier), 1.f, 0.001f);

	FMinimalViewInfo View = ViewAt(FVector::ZeroVector);
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("Final view pushed back so the next room is not visible"), View.Location.Equals(FVector(0.f, -500.f, 0.f), 0.05f));

	// 初始化后的第一帧会按位置直接到位一次，先在原区域消耗掉
	RoomView->TickComponent(0.f, LEVELTICK_All, &RoomView->PrimaryComponentTick);

	// 角色走进下一个区域：挡墙只在切换之后开始过渡，不会在同一帧瞬间失效
	Owner->SetActorLocation(FVector(0.f, 3000.f, 0.f));
	RoomView->TickComponent(0.f, LEVELTICK_All, &RoomView->PrimaryComponentTick);
	TestTrue(TEXT("Region switched"), RoomView->GetActiveRoomId() == FName(TEXT("Building.Corridor")));
	View = ViewAt(FVector::ZeroVector);
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("No instant release on the switch frame"), View.Location.Equals(FVector(0.f, -500.f, 0.f), 0.05f));

	RoomView->SnapToCurrentLocation();
	TestEqual(TEXT("Respawn / teleport snaps the weight"), RoomView->GetBarrierWeight(Barrier), 0.f, 0.001f);
	View = ViewAt(FVector::ZeroVector);
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("Inactive barrier leaves the view alone"), View.Location.Equals(FVector::ZeroVector, 0.01f));

	FindFProperty<FBoolProperty>(UTARoomViewComponent::StaticClass(), TEXT("bCameraBarriersEnabled"))->SetPropertyValue_InContainer(RoomView, false);
	Owner->SetActorLocation(FVector::ZeroVector);
	RoomView->SnapToCurrentLocation();
	View = ViewAt(FVector::ZeroVector);
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("Disabling barriers on the view component bypasses them"), View.Location.Equals(FVector::ZeroVector, 0.01f));
	return true;
}
#endif
