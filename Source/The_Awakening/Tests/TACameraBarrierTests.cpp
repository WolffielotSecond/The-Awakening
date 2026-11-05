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

		/** 挡墙在原点，厚度方向为 +X，半厚 20，宽 ±500（Y），高 ±300（Z），摄像机半径 10。 */
		ATACameraBarrier* Barrier()
		{
			ATACameraBarrier* B = World->SpawnActor<ATACameraBarrier>(FVector::ZeroVector, FRotator::ZeroRotator);
			B->GetBarrierBox()->SetBoxExtent(FVector(20.f, 500.f, 300.f));
			B->CameraRadiusCm = 10.f;
			B->CrossSeconds = 0.5f;
			Registry->RegisterBarrier(B);
			return B;
		}
	};

	FMinimalViewInfo ViewAt(const FVector& Location)
	{
		FMinimalViewInfo View;
		View.Location = Location;
		return View;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierPushTest, "TheAwakening.Camera.Barrier.PushAndSides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierPushTest::RunTest(const FString&)
{
	TACameraBarrierTests::FWorldFixture F;
	ATACameraBarrier* B = F.Barrier();
	TestEqual(TEXT("Camera clear of the barrier is not touched"), B->ComputePush(FVector(100.f, 0.f, 0.f), 1), 0.f, 0.001f);
	TestEqual(TEXT("Camera touching the face stops at radius distance"), B->ComputePush(FVector(25.f, 0.f, 0.f), 1), 5.f, 0.01f);
	TestEqual(TEXT("Camera pushed through by look input is pushed back to its side"), B->ComputePush(FVector(-100.f, 0.f, 0.f), 1), 130.f, 0.01f);
	TestEqual(TEXT("Two-sided: the other side blocks the same way"), B->ComputePush(FVector(-25.f, 0.f, 0.f), -1), 5.f, 0.01f);
	TestEqual(TEXT("Camera beyond the barrier width cannot hit it"), B->ComputePush(FVector(0.f, 600.f, 0.f), 1), 0.f, 0.001f);
	TestEqual(TEXT("Camera above the barrier cannot hit it"), B->ComputePush(FVector(0.f, 0.f, 400.f), 1), 0.f, 0.001f);

	TestEqual(TEXT("First side from the center plane (+)"), B->ResolvePlayerSide(FVector(5.f, 0.f, 0.f), 0), 1);
	TestEqual(TEXT("First side from the center plane (-)"), B->ResolvePlayerSide(FVector(-5.f, 0.f, 0.f), 0), -1);
	TestEqual(TEXT("Inside the thickness keeps the previous side"), B->ResolvePlayerSide(FVector(-15.f, 0.f, 0.f), 1), 1);
	TestEqual(TEXT("Fully past the other face switches side"), B->ResolvePlayerSide(FVector(-25.f, 0.f, 0.f), 1), -1);
	TestEqual(TEXT("Hysteresis works both ways"), B->ResolvePlayerSide(FVector(15.f, 0.f, 0.f), -1), -1);
	TestEqual(TEXT("Switches back once past the face"), B->ResolvePlayerSide(FVector(25.f, 0.f, 0.f), -1), 1);

	B->bBarrierEnabled = false;
	TestEqual(TEXT("Disabled barrier never pushes"), B->ComputePush(FVector(-100.f, 0.f, 0.f), 1), 0.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierCrossTest, "TheAwakening.Camera.Barrier.CrossThroughBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierCrossTest::RunTest(const FString&)
{
	TACameraBarrierTests::FWorldFixture F;
	ATACameraBarrier* B = F.Barrier();
	// 玩家刚换到 + 侧，摄像机还在墙中心：从挡在旧一侧（-30）平滑过渡到挡在新一侧（+30）
	const FVector Camera = FVector::ZeroVector;
	TestTrue(TEXT("Start of the crossing keeps the camera on the old side"), (Camera + B->ComputeDisplacement(Camera, 1, 0.f)).Equals(FVector(-30.f, 0.f, 0.f), 0.01f));
	TestTrue(TEXT("Middle of the crossing passes through the wall"), (Camera + B->ComputeDisplacement(Camera, 1, 0.5f)).Equals(FVector::ZeroVector, 0.01f));
	TestTrue(TEXT("End of the crossing is held on the new side"), (Camera + B->ComputeDisplacement(Camera, 1, 1.f)).Equals(FVector(30.f, 0.f, 0.f), 0.01f));

	float Previous = -TNumericLimits<float>::Max();
	bool bMonotonic = true;
	for (int32 Step = 0; Step <= 10; ++Step)
	{
		const float X = static_cast<float>((Camera + B->ComputeDisplacement(Camera, 1, Step / 10.f)).X);
		if (X < Previous - 0.01f) bMonotonic = false;
		Previous = X;
	}
	TestTrue(TEXT("Crossing never jumps back"), bMonotonic);

	const FVector Follow(80.f, 0.f, 0.f);
	TestTrue(TEXT("Camera already past the new face is released smoothly from the old side"),
		(Follow + B->ComputeDisplacement(Follow, 1, 0.5f)).Equals(FVector(80.f - 0.5f * 110.f, 0.f, 0.f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACameraBarrierRoomViewTest, "TheAwakening.Camera.Barrier.RoomViewCrossing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACameraBarrierRoomViewTest::RunTest(const FString&)
{
	using namespace TACameraBarrierTests;
	FWorldFixture F;
	ATACameraBarrier* Barrier = F.Barrier();

	AActor* Owner = F.World->SpawnActor<AActor>(FVector(-200.f, 0.f, 0.f), FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	Owner->SetActorLocation(FVector(-200.f, 0.f, 0.f));
	UTARoomViewComponent* RoomView = NewObject<UTARoomViewComponent>(Owner);
	RoomView->RegisterComponent();
	RoomView->Initialize(nullptr, nullptr, FTARoomCameraSettings());
	// 初始化后的第一帧会按位置直接到位一次，先消耗掉
	F.World->DeltaTimeSeconds = 0.f;
	RoomView->TickComponent(0.f, LEVELTICK_All, &RoomView->PrimaryComponentTick);
	TestEqual(TEXT("Player starts on the back side"), RoomView->GetBarrierSide(Barrier), -1);

	FMinimalViewInfo View = ViewAt(FVector(-15.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("Camera stops at the barrier on the player's side"), View.Location.Equals(FVector(-30.f, 0.f, 0.f), 0.01f));

	Owner->SetActorLocation(FVector(-5.f, 0.f, 0.f));
	RoomView->TickComponent(0.f, LEVELTICK_All, &RoomView->PrimaryComponentTick);
	TestEqual(TEXT("Standing in the doorway keeps the side"), RoomView->GetBarrierSide(Barrier), -1);

	Owner->SetActorLocation(FVector(100.f, 0.f, 0.f));
	RoomView->TickComponent(0.f, LEVELTICK_All, &RoomView->PrimaryComponentTick);
	TestEqual(TEXT("Walking out on the other side switches"), RoomView->GetBarrierSide(Barrier), 1);
	View = ViewAt(FVector(50.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("No jump on the switch frame: camera still held on the old side"), View.Location.Equals(FVector(-30.f, 0.f, 0.f), 0.01f));

	F.World->DeltaTimeSeconds = 0.25f;
	RoomView->TickComponent(0.25f, LEVELTICK_All, &RoomView->PrimaryComponentTick);
	View = ViewAt(FVector(50.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(*FString::Printf(TEXT("Halfway the camera is passing through the wall (x = %.1f)"), View.Location.X), View.Location.Equals(FVector(10.f, 0.f, 0.f), 0.01f));

	RoomView->TickComponent(0.25f, LEVELTICK_All, &RoomView->PrimaryComponentTick);
	View = ViewAt(FVector(50.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("After CrossSeconds the camera follows on the new side"), View.Location.Equals(FVector(50.f, 0.f, 0.f), 0.01f));
	View = ViewAt(FVector(15.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("And is now blocked on the new side"), View.Location.Equals(FVector(30.f, 0.f, 0.f), 0.01f));
	F.World->DeltaTimeSeconds = 0.f;

	Owner->SetActorLocation(FVector(-200.f, 0.f, 0.f));
	RoomView->SnapToCurrentLocation();
	TestEqual(TEXT("Respawn / teleport snaps the crossing"), RoomView->GetBarrierCrossAlpha(Barrier), 1.f, 0.001f);
	View = ViewAt(FVector(-15.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("Snapped straight onto the player's side"), View.Location.Equals(FVector(-30.f, 0.f, 0.f), 0.01f));

	FindFProperty<FBoolProperty>(UTARoomViewComponent::StaticClass(), TEXT("bCameraBarriersEnabled"))->SetPropertyValue_InContainer(RoomView, false);
	View = ViewAt(FVector(-15.f, 0.f, 0.f));
	RoomView->ApplyCameraBarriers(View);
	TestTrue(TEXT("Disabling barriers on the view component bypasses them"), View.Location.Equals(FVector(-15.f, 0.f, 0.f), 0.01f));

	F.Registry->UnregisterBarrier(Barrier);
	TestEqual(TEXT("Unregistered barrier is removed"), F.Registry->GetBarriers().Num(), 0);
	return true;
}
#endif
