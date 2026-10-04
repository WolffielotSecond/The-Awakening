#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningPlayerController.h"
#include "The_AwakeningCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "InputModifiers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHeldObservationLifecycleTest,
	"TheAwakening.Input.HeldObservationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAHeldObservationLifecycleTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC) || !TestTrue(TEXT("Slate initialized for real Up delivery"), FSlateApplication::IsInitialized()))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Local = NewObject<ULocalPlayer>(GEngine);
	Local->PlayerController = PC; PC->Player = Local;
	UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	auto* Pawn = PawnClass ? World->SpawnActor<AThe_AwakeningCharacter>(PawnClass) : nullptr;
	if (!TestNotNull(TEXT("Character"), Pawn)) { World->DestroyWorld(false); return false; }
	PC->Possess(Pawn);
	const int32 User = Local->GetControllerId();
	FTAInputDeviceDetector Processor(PC);
	auto* Modifier = NewObject<UInputModifierNegate>(PC);
	int32 LossEvents = 0;
	PC->ObservePlayerInputOwnership(TAInputOwnershipAdapter::EState::Owned);
	const auto LossListener = PC->OnPlayerInputOwnershipLost.AddLambda([&]() { ++LossEvents; });

	auto SeedObservation = [&]()
	{
		PC->RecordHeldInput(EKeys::W, 1.f, User);
		PC->RecordHeldInput(EKeys::Tab, 1.f, User);
		PC->RecordHeldInput(EKeys::Gamepad_LeftX, .7f, User);
		PC->RecordHeldInput(EKeys::Gamepad_LeftY, .4f, User);
		PC->RecordHeldInput(EKeys::Gamepad_Special_Left, 1.f, User);
		PC->ConsumedInventoryKeys = {EKeys::Tab, EKeys::Gamepad_Special_Left};
		PC->HeldInputModifiers.Add(Modifier, Modifier);
		PC->RecordHeldInput(EKeys::Gamepad_RightY, .8f, User);
		Processor.ConsumedPresses = {EKeys::Tab, EKeys::Gamepad_Special_Left};
		Pawn->GetCharacterMovement()->MaxWalkSpeed = 750.f;
	};

	// A real release affects only its key and press latch, even while Move is denied.
	FTAInputRequest UIRequest; UIRequest.Owner = PC; UIRequest.Priority = 200;
	UIRequest.Allowed = {ETAInputCapability::Navigate};
	const auto Handle = PC->AcquireInputRequest(UIRequest);
	SeedObservation();
	using TAInputOwnershipAdapter::EState;
	const FVector HeldStickBefore = PC->ReadHeldKey(EKeys::Gamepad_Left2D);
	const FVector2D CursorBefore = PC->ReadCursorStick(EKeys::Gamepad_Left2D);
	Pawn->AddMovementInput(FVector::ForwardVector, 1.f, true);
	const FVector TransitionPendingBefore = Pawn->GetPendingMovementInputVector();
	for (int32 GapObservation = 0; GapObservation < 5; ++GapObservation)
	{
		PC->ObservePlayerInputOwnership(EState::Transition);
		// Run the production analog observation path with the adapter's unresolved state.
		TestFalse(TEXT("Transition does not accept analog commands or zero samples"),
			PC->ObserveAnalogInput(EKeys::Gamepad_LeftX, 0.f, User, EState::Transition));
	}
	TestFalse(TEXT("Transition preserves physical W"), PC->ReadHeldKey(EKeys::W).IsNearlyZero());
	TestEqual(TEXT("Transition analog does not fake a paired-axis zero"), PC->ReadHeldKey(EKeys::Gamepad_Left2D), HeldStickBefore);
	TestEqual(TEXT("Transition preserves derived cursor axis"), PC->ReadCursorStick(EKeys::Gamepad_Left2D), CursorBefore);
	TestEqual(TEXT("Transition preserves accepted speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	TestEqual(TEXT("Transition does not consume pending command"), Pawn->GetPendingMovementInputVector(), TransitionPendingBefore);
	TestEqual(TEXT("Transition retains modifiers"), PC->HeldInputModifiers.Num(), 1);
	TestEqual(TEXT("Transition retains Inventory latches"), PC->ConsumedInventoryKeys.Num(), 2);
	TestEqual(TEXT("Transition is not an ownership loss edge"), LossEvents, 0);
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestEqual(TEXT("Owned recovery preserves observed stick without rebuilding it"), PC->ReadHeldKey(EKeys::Gamepad_Left2D), HeldStickBefore);
	Pawn->ConsumeMovementInputVector();

	PC->ObservePlayerInputOwnership(EState::Transition);
	TestFalse(TEXT("Lost rejects the analog sample"), PC->ObserveAnalogInput(EKeys::Gamepad_LeftX, 1.f, User, EState::Lost));
	TestTrue(TEXT("Transition to Lost invalidates rather than recording a physical zero"), PC->HeldKeyValues.IsEmpty());
	TestTrue(TEXT("Lost invalidates modifier instances"), PC->HeldInputModifiers.IsEmpty());
	TestTrue(TEXT("Lost invalidates Inventory latches"), PC->ConsumedInventoryKeys.IsEmpty());
	TestEqual(TEXT("Lost uses existing accepted-command cleanup"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.f);
	TestEqual(TEXT("Owned through Transition to Lost emits once"), LossEvents, 1);
	// A repeated Lost observation must not rerun the invalidation or notify again.
	Pawn->GetCharacterMovement()->MaxWalkSpeed = 750.f;
	PC->ObservePlayerInputOwnership(EState::Lost);
	TestFalse(TEXT("Repeated Lost still rejects analog without recording zero"),
		PC->ObserveAnalogInput(EKeys::Gamepad_LeftX, 0.f, User, EState::Lost));
	TestEqual(TEXT("Repeated Lost does not repeat accepted-command cleanup"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	TestEqual(TEXT("Repeated Lost emits no additional edge"), LossEvents, 1);
	TestTrue(TEXT("Repeated Lost leaves observation absent, not zero samples"), PC->HeldKeyValues.IsEmpty());
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestTrue(TEXT("Owned recovery does not synthesize Press or Release"), PC->HeldKeyValues.IsEmpty());
	TestEqual(TEXT("Owned recovery does not synthesize pairing Up"), Processor.ConsumedPresses.Num(), 2);
	TestEqual(TEXT("Loss retains persistent winner"), PC->GetInputWinner().Handle, Handle);
	TestTrue(TEXT("Owned accepts a new real analog sample"), PC->ObserveAnalogInput(EKeys::Gamepad_LeftX, .6f, User, EState::Owned));
	TestEqual(TEXT("New physical sample updates observation"), PC->ReadHeldKey(EKeys::Gamepad_LeftX).X, static_cast<double>(.6f));
	TestTrue(TEXT("Owned accepts a real analog zero"), PC->ObserveAnalogInput(EKeys::Gamepad_LeftX, 0.f, User, EState::Owned));
	TestTrue(TEXT("Real analog zero remains an observed key"), PC->HeldKeyValues.Contains(EKeys::Gamepad_LeftX));
	TestTrue(TEXT("Real analog zero records zero"), PC->ReadHeldKey(EKeys::Gamepad_LeftX).IsNearlyZero());
	LossEvents = 0;
	SeedObservation();
	TestFalse(TEXT("UI denies Move"), PC->AllowsInput(ETAInputCapability::Move));
	TestFalse(TEXT("Denial preserves physical W"), PC->ReadHeldKey(EKeys::W).IsNearlyZero());
	FKeyEvent TabUp(EKeys::Tab, FModifierKeysState(), User, false, 0, 0);
	TestTrue(TEXT("Matching Up is consumed once"), Processor.HandleKeyUpEvent(FSlateApplication::Get(), TabUp));
	TestFalse(TEXT("Repeated Up is not paired twice"), Processor.HandleKeyUpEvent(FSlateApplication::Get(), TabUp));
	TestTrue(TEXT("Physical Tab release records zero"), PC->ReadHeldKey(EKeys::Tab).IsNearlyZero());
	TestFalse(TEXT("Release rearms only the Tab latch"), PC->ConsumedInventoryKeys.Contains(EKeys::Tab));
	TestTrue(TEXT("Release preserves gamepad latch"), PC->ConsumedInventoryKeys.Contains(EKeys::Gamepad_Special_Left));
	TestTrue(TEXT("Release preserves other Down/Up pairing"), Processor.ConsumedPresses.Contains(EKeys::Gamepad_Special_Left));
	TestEqual(TEXT("Release does not reset modifier instances"), PC->HeldInputModifiers.Num(), 1);
	TestEqual(TEXT("Release does not clear accepted speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	TestEqual(TEXT("Release does not release UI ownership"), PC->GetInputWinner().Handle, Handle);

	// Exercise the exact cleanup called after local-player device disconnect filtering.
	SeedObservation();
	PC->InvalidateGamepadObservationOnDisconnect();
	PC->InvalidateGamepadObservationOnDisconnect();
	TestFalse(TEXT("Disconnect retains keyboard Held"), PC->ReadHeldKey(EKeys::W).IsNearlyZero());
	TestTrue(TEXT("Disconnect removes scalar gamepad observation"), !PC->HeldKeyValues.Contains(EKeys::Gamepad_LeftX));
	TestTrue(TEXT("Disconnect removes paired gamepad observation"), !PC->HeldKeyValues.Contains(EKeys::Gamepad_Left2D));
	TestTrue(TEXT("Disconnect retains keyboard toggle latch"), PC->ConsumedInventoryKeys.Contains(EKeys::Tab));
	TestFalse(TEXT("Disconnect removes gamepad toggle latch"), PC->ConsumedInventoryKeys.Contains(EKeys::Gamepad_Special_Left));
	TestEqual(TEXT("Disconnect preserves modifier lifetime"), PC->HeldInputModifiers.Num(), 1);
	TestEqual(TEXT("Disconnect preserves accepted speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	TestTrue(TEXT("Disconnect invalidates the source of both cursor axes"), PC->ReadCursorStick(EKeys::Gamepad_Left2D).IsNearlyZero() && PC->ReadCursorStick(EKeys::Gamepad_Right2D).IsNearlyZero());
	TestEqual(TEXT("Disconnect does not synthesize pairing releases"), Processor.ConsumedPresses.Num(), 2);
	TestEqual(TEXT("Disconnect is not global ownership loss"), LossEvents, 0);

	// Global invalidation preserves current external-loss behavior, not an active Stop.
	SeedObservation();
	Pawn->GetCharacterMovement()->Velocity = FVector(321.f, 0.f, 0.f);
	Pawn->AddMovementInput(FVector::ForwardVector, 1.f, true);
	const FVector PendingBefore = Pawn->GetPendingMovementInputVector();
	PC->InvalidatePhysicalObservationForExternalOwnership();
	PC->InvalidatePhysicalObservationForExternalOwnership();
	TestTrue(TEXT("Global invalidation removes all observed keys"), PC->HeldKeyValues.IsEmpty());
	TestTrue(TEXT("Global invalidation resets all toggle latches"), PC->ConsumedInventoryKeys.IsEmpty());
	TestTrue(TEXT("Global invalidation resets modifier instances"), PC->HeldInputModifiers.IsEmpty());
	TestTrue(TEXT("Global invalidation invalidates the source of both cursor axes"), PC->ReadCursorStick(EKeys::Gamepad_Left2D).IsNearlyZero() && PC->ReadCursorStick(EKeys::Gamepad_Right2D).IsNearlyZero());
	TestEqual(TEXT("Existing external cleanup clears accepted speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.f);
	TestEqual(TEXT("Invalidation does not Stop velocity"), Pawn->GetCharacterMovement()->Velocity, FVector(321.f, 0.f, 0.f));
	TestEqual(TEXT("Invalidation does not consume pending gameplay input"), Pawn->GetPendingMovementInputVector(), PendingBefore);
	TestEqual(TEXT("Observation invalidation itself does not broadcast loss"), LossEvents, 0);
	TestEqual(TEXT("Invalidation does not change persistent winner"), PC->GetInputWinner().Handle, Handle);
	TestEqual(TEXT("Global invalidation preserves Down/Up pairing"), Processor.ConsumedPresses.Num(), 2);
	TestTrue(TEXT("Outstanding Up remains paired after invalidation"), Processor.HandleKeyUpEvent(FSlateApplication::Get(), TabUp));
	TestFalse(TEXT("Outstanding Up remains exactly once"), Processor.HandleKeyUpEvent(FSlateApplication::Get(), TabUp));

	// Activation still invalidates before the independent, once-only loss notification.
	SeedObservation();
	PC->NotifyApplicationActivationChanged(false);
	PC->NotifyApplicationActivationChanged(false);
	TestTrue(TEXT("Repeated deactivation leaves observation empty"), PC->HeldKeyValues.IsEmpty());
	TestEqual(TEXT("Repeated deactivation emits one loss edge"), LossEvents, 1);
	TestEqual(TEXT("Deactivation preserves persistent request"), PC->GetInputWinner().Handle, Handle);
	PC->NotifyApplicationActivationChanged(true);
	TestTrue(TEXT("Reactivation does not rebuild physical Held"), PC->HeldKeyValues.IsEmpty());
	TestEqual(TEXT("Reactivation does not synthesize pairing Up"), Processor.ConsumedPresses.Num(), 2);
	PC->OnPlayerInputOwnershipLost.Remove(LossListener);
	PC->ReleaseInputRequest(Handle);
	World->DestroyWorld(false);
	return true;
}
#endif
