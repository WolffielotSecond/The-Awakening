#include "Camera/TAAutoFocusCameraComponent.h"
#include "The_AwakeningCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TAAutoFocusTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AActor* Owner = nullptr;
		UTAAutoFocusCameraComponent* Camera = nullptr;
		USceneComponent* Point = nullptr;

		FFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Owner = World->SpawnActor<AActor>();
			Camera = NewObject<UTAAutoFocusCameraComponent>(Owner);
			Camera->RegisterComponent();
			Point = NewObject<USceneComponent>(Owner);
			Point->RegisterComponent();
			Camera->FocusPoint = Point;
		}

		~FFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAAutoFocusViewTest, "TheAwakening.Camera.AutoFocus.ViewAndSmoothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAAutoFocusViewTest::RunTest(const FString&)
{
	TAAutoFocusTests::FFixture F;
	F.Camera->SetWorldLocationAndRotation(FVector(100.f, 200.f, 50.f), FRotator(0.f, 90.f, 0.f));
	F.Point->SetWorldLocation(FVector(400.f, 1000.f, 150.f));
	F.Camera->PostProcessSettings.DepthOfFieldFocalDistance = 321.f;
	F.Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = false;
	F.Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
	F.Camera->PostProcessSettings.DepthOfFieldFstop = 2.8f;
	F.Camera->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
	F.Camera->PostProcessSettings.DepthOfFieldSensorWidth = 24.f;
	F.Camera->PostProcessBlendWeight = 0.75f;
	F.Camera->FocusInterpSpeed = 2.f;
	FMinimalViewInfo View;
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Initial focus is optical projection, independent of lateral/vertical displacement"), F.Camera->CurrentFocalDistance, 800.f);
	TestTrue(TEXT("Only focal-distance override is enabled by autofocus"), View.PostProcessSettings.bOverride_DepthOfFieldFocalDistance);
	TestEqual(TEXT("Aperture preserved"), View.PostProcessSettings.DepthOfFieldFstop, 2.8f);
	TestEqual(TEXT("Sensor width preserved"), View.PostProcessSettings.DepthOfFieldSensorWidth, 24.f);
	TestEqual(TEXT("Blend weight preserved"), View.PostProcessBlendWeight, 0.75f);
	TestEqual(TEXT("Authored manual focus preserved"), F.Camera->PostProcessSettings.DepthOfFieldFocalDistance, 321.f);
	TestFalse(TEXT("Authored manual override preserved"), F.Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance);

	F.Camera->SetWorldLocation(FVector(100.f, 400.f, 50.f));
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Moving camera uses actual updated world position"), F.Camera->TargetFocalDistance, 600.f);
	TestEqual(TEXT("FInterpTo smooths 800 toward 600 with alpha 0.2"), F.Camera->CurrentFocalDistance, 760.f);
	F.Point->SetWorldLocation(FVector(400.f, 900.f, 150.f));
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Moving character focus point updates target"), F.Camera->TargetFocalDistance, 500.f);
	TestEqual(TEXT("Focus continues smoothing"), F.Camera->CurrentFocalDistance, 708.f);

	F.Camera->FocusInterpSpeed = 0.f;
	F.Camera->AddAdditiveOffset(FTransform(FVector(100.f, 0.f, 0.f)), 0.f);
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Actual view includes camera-local additive offset"), F.Camera->CurrentFocalDistance, 400.f);
	F.Camera->ClearAdditiveOffset();
	F.Point->SetWorldLocation(FVector(400.f, 300.f, 150.f));
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Behind-camera target clamps to positive focus, never zero DOF-disable sentinel"), F.Camera->CurrentFocalDistance, 1.f);

	F.Camera->bAutoFocusEnabled = false;
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Disabling restores manual focal distance"), View.PostProcessSettings.DepthOfFieldFocalDistance, 321.f);
	TestFalse(TEXT("Disabling restores manual override"), View.PostProcessSettings.bOverride_DepthOfFieldFocalDistance);
	F.Camera->bAutoFocusEnabled = true;
	F.Camera->FocusPoint = nullptr;
	F.Camera->GetCameraView(0.1f, View);
	TestEqual(TEXT("Missing target uses manual focus"), View.PostProcessSettings.DepthOfFieldFocalDistance, 321.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAAutoFocusCharacterTest, "TheAwakening.Camera.AutoFocus.CharacterBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAAutoFocusCharacterTest::RunTest(const FString&)
{
	TAAutoFocusTests::FFixture F;
	UClass* CharacterClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (!TestNotNull(TEXT("Existing character Blueprint loads"), CharacterClass)) return false;
	auto* Character = F.World->SpawnActor<AThe_AwakeningCharacter>(CharacterClass);
	if (!TestNotNull(TEXT("Existing character Blueprint spawns"), Character)) return false;
	auto* Camera = Character->GetFollowCamera();
	if (!TestNotNull(TEXT("Existing FollowCamera uses autofocus subclass"), Camera)) return false;
	if (!TestNotNull(TEXT("Chest focus point is bound on Blueprint instance"), Camera->FocusPoint.Get())) return false;
	TestTrue(TEXT("Focus point belongs to spawned character"), Camera->FocusPoint->GetOwner() == Character);
	TestTrue(TEXT("Existing spring arm reused"), Camera->GetAttachParent() == Character->GetCameraBoom());
	Camera->SetWorldLocation(Camera->FocusPoint->GetComponentLocation() - FVector(900.f, 200.f, 100.f));
	Camera->SetWorldRotation(FRotator::ZeroRotator);
	FMinimalViewInfo View;
	Camera->GetCameraView(1.f / 60.f, View);
	TestTrue(TEXT("Focus follows actual optical distance rather than boom length"), FMath::IsNearlyEqual(Camera->TargetFocalDistance, 900.f, 0.01f));
	return true;
}
#endif
