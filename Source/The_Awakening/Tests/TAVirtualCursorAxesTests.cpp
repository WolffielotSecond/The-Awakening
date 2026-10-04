#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAVirtualCursorAxesTest, "TheAwakening.Input.VirtualCursorAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAVirtualCursorAxesTest::RunTest(const FString&)
{
	using TAInputOwnershipAdapter::EState;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC) || !TestTrue(TEXT("Slate event delivery available"), FSlateApplication::IsInitialized()))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	const int32 User = Local->GetControllerId();
	FTAInputDeviceDetector Processor(PC);
	auto Analog = [&](FKey Key, float Value)
	{
		FAnalogInputEvent Event(Key, FModifierKeysState(), User, false, 0, 0, Value);
		return Processor.HandleAnalogInputEvent(FSlateApplication::Get(), Event);
	};
	auto AssertRemappedAxis = [&](const TCHAR* Label, FVector2D Actual, FVector2D Expected)
	{
		// The original remap uses float arithmetic; check analytic .5 to float precision.
		TestEqual(FString(Label) + TEXT(" X"), Actual.X, Expected.X, 1.e-7);
		TestEqual(FString(Label) + TEXT(" Y"), Actual.Y, Expected.Y, 1.e-7);
	};

	struct FStickKeys { FKey X, Y, Pair; };
	for (const FStickKeys Stick : {
		FStickKeys{EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY, EKeys::Gamepad_Left2D},
		FStickKeys{EKeys::Gamepad_RightX, EKeys::Gamepad_RightY, EKeys::Gamepad_Right2D}})
	{
		Analog(Stick.X, .59f);
		TestEqual(TEXT("Single axis updates raw observation"), PC->ReadHeldKey(Stick.X), FVector(.59f, 0.f, 0.f));
		AssertRemappedAxis(TEXT("Single axis derives cursor with original remap"), PC->ReadCursorStick(Stick.Pair), FVector2D(.5, 0.));
		Analog(Stick.Y, -.59f);
		TestEqual(TEXT("Separate events update paired Held axes"), PC->ReadHeldKey(Stick.Pair), FVector(.59f, -.59f, 0.f));
		AssertRemappedAxis(TEXT("Both components use per-axis dead zone"), PC->ReadCursorStick(Stick.Pair), FVector2D(.5, -.5));
		Analog(Stick.X, 0.f);
		TestEqual(TEXT("Real X zero preserves Y"), PC->ReadHeldKey(Stick.Pair), FVector(0.f, -.59f, 0.f));
		AssertRemappedAxis(TEXT("Real X zero immediately updates derived cursor"), PC->ReadCursorStick(Stick.Pair), FVector2D(0., -.5));
		Analog(Stick.Y, 0.f);
		TestTrue(TEXT("Centered stick derives zero"), PC->ReadCursorStick(Stick.Pair).IsNearlyZero());
		TestTrue(TEXT("Real zero remains an observation, not invalidation"), PC->HeldKeyValues.Contains(Stick.X) && PC->HeldKeyValues.Contains(Stick.Y));
		Analog(Stick.X, .18f); Analog(Stick.Y, -.18f);
		TestTrue(TEXT("Dead zone is per component, not radial"), PC->ReadCursorStick(Stick.Pair).IsNearlyZero());
		Analog(Stick.X, 0.f); Analog(Stick.Y, 0.f);
	}

	// Both sticks are already held before any cursor consumer is enabled.
	Analog(EKeys::Gamepad_LeftX, .59f);
	Analog(EKeys::Gamepad_RightY, .59f);
	PC->RecordHeldInput(EKeys::W, 1.f, User);
	const FVector LeftBefore = PC->ReadHeldKey(EKeys::Gamepad_Left2D);
	const FVector RightBefore = PC->ReadHeldKey(EKeys::Gamepad_Right2D);
	const int32 KeyCountBefore = PC->HeldKeyValues.Num();
	auto AssertObservationUnchanged = [&]()
	{
		TestEqual(TEXT("Consumer switching preserves left physical stick"), PC->ReadHeldKey(EKeys::Gamepad_Left2D), LeftBefore);
		TestEqual(TEXT("Consumer switching preserves right physical stick"), PC->ReadHeldKey(EKeys::Gamepad_Right2D), RightBefore);
		TestEqual(TEXT("Consumer switching does not reconstruct observed keys"), PC->HeldKeyValues.Num(), KeyCountBefore);
		TestFalse(TEXT("Consumer switching preserves physical W"), PC->ReadHeldKey(EKeys::W).IsNearlyZero());
	};
	TestFalse(TEXT("Base has no cursor permission"), PC->AllowsInput(ETAInputCapability::Cursor));
	PC->TickVirtualCursor(.016f);
	AssertObservationUnchanged();

	FTAInputRequest Scan; Scan.Owner = PC; Scan.Priority = 100;
	Scan.Allowed = {ETAInputCapability::Look, ETAInputCapability::Cursor};
	const auto ScanHandle = PC->AcquireInputRequest(Scan);
	TestTrue(TEXT("Cursor+Look is active without a new analog event"), PC->IsCursorStickLookActive());
	AssertRemappedAxis(TEXT("Cursor+Look combines both sticks"), PC->GetCursorInputAxis(), FVector2D(.5, .5));
	AssertObservationUnchanged();
	FTAInputRequest Menu; Menu.Owner = PC; Menu.Priority = 200;
	Menu.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor};
	const auto MenuHandle = PC->AcquireInputRequest(Menu);
	TestFalse(TEXT("Menu stops cursor-look without clearing sticks"), PC->IsCursorStickLookActive());
	AssertRemappedAxis(TEXT("Menu cursor uses LS only"), PC->GetCursorInputAxis(), FVector2D(.5, 0.));
	AssertObservationUnchanged();
	FTAInputRequest Covered; Covered.Owner = PC; Covered.Priority = 300;
	Covered.Allowed = {ETAInputCapability::Pan};
	const auto CoveredHandle = PC->AcquireInputRequest(Covered);
	TestFalse(TEXT("Cover denies cursor consumption"), PC->AllowsInput(ETAInputCapability::Cursor));
	PC->TickVirtualCursor(.016f);
	AssertObservationUnchanged();
	PC->ReleaseInputRequest(CoveredHandle);
	AssertRemappedAxis(TEXT("Uncover resumes menu cursor without new samples"), PC->GetCursorInputAxis(), FVector2D(.5, 0.));
	PC->ReleaseInputRequest(MenuHandle);
	AssertRemappedAxis(TEXT("Uncover resumes both Cursor+Look axes without reseed"), PC->GetCursorInputAxis(), FVector2D(.5, .5));
	AssertObservationUnchanged();

	Analog(EKeys::Gamepad_LeftX, 1.f); Analog(EKeys::Gamepad_LeftY, 1.f);
	Analog(EKeys::Gamepad_RightX, 1.f); Analog(EKeys::Gamepad_RightY, 1.f);
	TestTrue(TEXT("Combined cursor still clamps magnitude to one"), FMath::IsNearlyEqual(PC->GetCursorInputAxis().Size(), 1.));
	Analog(EKeys::Gamepad_LeftY, 0.f); Analog(EKeys::Gamepad_RightY, 0.f);
	Analog(EKeys::Gamepad_RightX, -1.f);
	TestTrue(TEXT("Opposed sticks cancel cursor movement"), PC->GetCursorInputAxis().IsNearlyZero());
	TestTrue(TEXT("Opposed sticks retain original mouse-warp suppression"), PC->IsCursorStickLookActive());

	const FVector2D LeftBeforeTransition = PC->ReadCursorStick(EKeys::Gamepad_Left2D);
	PC->ObservePlayerInputOwnership(EState::Transition);
	TestFalse(TEXT("Transition ignores analog zero rather than clearing source"), PC->ObserveAnalogInput(EKeys::Gamepad_LeftX, 0.f, User, EState::Transition));
	TestEqual(TEXT("Transition retains derived cursor axis"), PC->ReadCursorStick(EKeys::Gamepad_Left2D), LeftBeforeTransition);
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestEqual(TEXT("Recovery retains derived axis without reseeding"), PC->ReadCursorStick(EKeys::Gamepad_Left2D), LeftBeforeTransition);
	PC->ObservePlayerInputOwnership(EState::Lost);
	TestTrue(TEXT("Lost invalidates the only axis source"), PC->HeldKeyValues.IsEmpty());
	TestTrue(TEXT("Lost leaves both derived axes zero"), PC->ReadCursorStick(EKeys::Gamepad_Left2D).IsNearlyZero() && PC->ReadCursorStick(EKeys::Gamepad_Right2D).IsNearlyZero());
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestTrue(TEXT("Recovery cannot invent cursor axes"), PC->GetCursorInputAxis().IsNearlyZero() && PC->HeldKeyValues.IsEmpty());
	PC->ReleaseInputRequest(ScanHandle);
	World->DestroyWorld(false);
	return true;
}
#endif
