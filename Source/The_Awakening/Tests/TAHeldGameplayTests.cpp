#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/TAParkourInputTestFixture.h"
#include "The_AwakeningPlayerController.h"
#include "The_AwakeningCharacter.h"
#include "Core/TAPlayerInput.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "Movement/TAParkourComponent.h"
#include "Movement/TAMovementComponent.h"
#include "Movement/TAParkourMarker.h"
#include "EnhancedInputSubsystemInterface.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "Components/SplineComponent.h"

namespace
{
	struct FHeldTestMappings : IEnhancedInputSubsystemInterface
	{
		UEnhancedPlayerInput* Input = nullptr;
		TMap<TObjectPtr<const UInputAction>, FInjectedInput> Injected;
		virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
		virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHeldGameplayTest, "TheAwakening.Input.HeldGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHeldGameplayTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (!TestNotNull(TEXT("Controller BP"), PCClass) || !TestNotNull(TEXT("Character BP"), PawnClass) || !TestNotNull(TEXT("Mapping context"), Context))
	{
		World->DestroyWorld(false); return false;
	}
	AThe_AwakeningPlayerController* PC = World->SpawnActor<AThe_AwakeningPlayerController>(PCClass);
	const FClassProperty* InputClassProperty = FindFProperty<FClassProperty>(PCClass, TEXT("OverridePlayerInputClass"));
	TestTrue(TEXT("Existing controller blueprint inherits held-input player class"),
		InputClassProperty && InputClassProperty->GetObjectPropertyValue_InContainer(PC) == UTAPlayerInput::StaticClass());
	AThe_AwakeningCharacter* Pawn = World->SpawnActor<AThe_AwakeningCharacter>(PawnClass);
	TestNotNull(TEXT("Existing character BP inherits landing movement component"), Cast<UTAMovementComponent>(Pawn->GetCharacterMovement()));
	ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
	Local->PlayerController = PC; PC->Player = Local;
	PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
	PC->Possess(Pawn);
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, 1.f, Local->GetControllerId());
	FTAInputRequest ScanRequest;
	ScanRequest.Owner = PC; ScanRequest.Priority = 100;
	ScanRequest.Allowed = {ETAInputCapability::Scan, ETAInputCapability::Look, ETAInputCapability::Cursor};
	ScanRequest.Presentation.bShowCursor = true;
	const auto ScanHandle = PC->AcquireInputRequest(ScanRequest);
	TestTrue(TEXT("Scan reads already-held stick cursor without menu mode"), PC->IsCursorStickLookActive() && PC->GetInputWinner().Request.Presentation.InputMode == ETAInputModeRequirement::GameOnly);
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, 0.f, Local->GetControllerId());
	TestFalse(TEXT("Centered scan stick stops cursor and explicit camera input"), PC->IsCursorStickLookActive());
	PC->RecordHeldInput(EKeys::Gamepad_RightY, 1.f, Local->GetControllerId());
	TestTrue(TEXT("Right stick also drives scan cursor and camera"), PC->IsCursorStickLookActive());
	PC->RecordHeldInput(EKeys::Gamepad_RightY, 0.f, Local->GetControllerId());
	TestFalse(TEXT("Right stick center stops scan cursor"), PC->IsCursorStickLookActive());
	PC->RecordHeldInput(EKeys::Gamepad_LeftY, 1.f, Local->GetControllerId());
	PC->ReleaseInputRequest(ScanHandle);
	TestFalse(TEXT("Scan exit disables stick cursor look"), PC->IsCursorStickLookActive());
	TestFalse(TEXT("Scan exit preserves physical stick observation"), PC->ReadHeldKey(EKeys::Gamepad_Left2D).IsNearlyZero());
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, 0.f, Local->GetControllerId());
	PC->RecordHeldInput(EKeys::Gamepad_LeftY, 0.f, Local->GetControllerId());
	// Input test has no level floor; bypass the unrelated ledge-safety traces.
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	FHeldTestMappings Mappings; Mappings.Input = Cast<UTAPlayerInput>(PC->PlayerInput);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	Mappings.AddMappingContext(Context, 0, Options);
	const int32 User = Local->GetControllerId();
	const UInputAction* Forward = nullptr;
	const UInputAction* Move = nullptr;
	const UInputAction* Sprint = nullptr;
	const UInputAction* InventoryAction = nullptr;
	for (const auto& Mapping : Context->GetMappings())
	{
		if (!Mapping.Action) continue;
		AddInfo(Mapping.Action->GetName() + TEXT(": ") + Mapping.Key.ToString());
		if (Mapping.Action->GetName() == TEXT("IA_MoveForward")) Forward = Mapping.Action;
		if (Mapping.Action->GetName() == TEXT("IA_Move")) Move = Mapping.Action;
		if (Mapping.Action->GetName() == TEXT("IA_Sprint")) Sprint = Mapping.Action;
		if (Mapping.Key == EKeys::Tab) InventoryAction = Mapping.Action;
	}
	if (!TestNotNull(TEXT("Forward mapping"), Forward) || !TestNotNull(TEXT("Move mapping"), Move) || !TestNotNull(TEXT("Sprint mapping"), Sprint))
	{
		World->DestroyWorld(false); return false;
	}
	FTAInputRequest MenuRequest;
	MenuRequest.Owner = Pawn; MenuRequest.Priority = 200;
	MenuRequest.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor};
	MenuRequest.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
	MenuRequest.Presentation.bShowCursor = true;
	FTAInputRouter::FHandle MenuHandle = 0;
	if (TestNotNull(TEXT("Inventory action mapping"), InventoryAction))
	{
		for (const FKey Key : {EKeys::Tab, EKeys::Gamepad_Special_Left})
		{
			PC->RecordHeldInput(Key, 1.f, User);
			TestTrue(TEXT("First inventory press accepted"), PC->ConsumeInventoryTogglePress(InventoryAction));
			MenuHandle = PC->AcquireInputRequest(MenuRequest);
			PC->RecordHeldInput(Key, 1.f, User);
			TestFalse(TEXT("Hold cannot close newly opened inventory"), PC->ConsumeInventoryTogglePress(InventoryAction));
			PC->RecordHeldInput(Key, 0.f, User);
			PC->RecordHeldInput(Key, 1.f, User);
			TestTrue(TEXT("New press can close inventory"), PC->ConsumeInventoryTogglePress(InventoryAction));
			PC->ReleaseInputRequest(MenuHandle);
			TestFalse(TEXT("Same closing press cannot reopen inventory"), PC->ConsumeInventoryTogglePress(InventoryAction));
			PC->RecordHeldInput(Key, 0.f, User);
		}
	}
	PC->RecordHeldInput(EKeys::W, 1.f, User);
	// Permission loss must not rewrite accepted motion, speed, or pending input.
	Pawn->GetCharacterMovement()->Velocity = FVector(500, 0, 0);
	Pawn->GetCharacterMovement()->MaxWalkSpeed = 750.f;
	Pawn->AddMovementInput(FVector::ForwardVector, 1.f, true);
	const FVector PendingBeforeDenial = Pawn->GetPendingMovementInputVector();
	MenuHandle = PC->AcquireInputRequest(MenuRequest);
	static_cast<AActor*>(Pawn)->Tick(.016f);
	TestEqual(TEXT("Gate preserves velocity"), Pawn->GetCharacterMovement()->Velocity, FVector(500, 0, 0));
	TestEqual(TEXT("Gate preserves configured speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	TestEqual(TEXT("Gate does not consume pending movement"), Pawn->GetPendingMovementInputVector(), PendingBeforeDenial);
	TestTrue(TEXT("Held observation is independent of permission"), PC->ReadHeldAction(Forward).IsNonZero());
	Pawn->ConsumeMovementInputVector();
	PC->ReleaseInputRequest(MenuHandle);
	MenuHandle = PC->AcquireInputRequest(MenuRequest);
	static_cast<AActor*>(Pawn)->Tick(.016f);
	TestTrue(TEXT("Menu prevents movement"), Pawn->GetPendingMovementInputVector().IsNearlyZero());
	PC->ReleaseInputRequest(MenuHandle);
	static_cast<AActor*>(Pawn)->Tick(.016f);
	TestFalse(TEXT("Still held keyboard resumes without new key-down"), Pawn->ConsumeMovementInputVector().IsNearlyZero());
	MenuHandle = PC->AcquireInputRequest(MenuRequest);
	PC->RecordHeldInput(EKeys::W, 0.f, User);
	PC->ReleaseInputRequest(MenuHandle); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestTrue(TEXT("Released in menu does not stick"), Pawn->ConsumeMovementInputVector().IsNearlyZero());
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, .4f, User);
	MenuHandle = PC->AcquireInputRequest(MenuRequest); PC->ReleaseInputRequest(MenuHandle); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestFalse(TEXT("Held stick resumes"), Pawn->ConsumeMovementInputVector().IsNearlyZero());
	TestEqual(TEXT("Partial stick walk speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.f);
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, 1.f, User); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestEqual(TEXT("Full stick does not automatically sprint"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.f);
	FKey SprintKey;
	for (const auto& Mapping : Context->GetMappings()) if (Mapping.Action == Sprint) { SprintKey = Mapping.Key; break; }
	PC->RecordHeldInput(SprintKey, 1.f, User); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestEqual(TEXT("Sprint action selects sprint speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, .4f, User); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestEqual(TEXT("Partial stick still has full sprint speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	PC->RecordHeldInput(SprintKey, 0.f, User);
	PC->RecordHeldInput(EKeys::Gamepad_LeftThumbstick, 1.f, User);
	Pawn->ConsumeMovementInputVector(); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestEqual(TEXT("L3 selects sprint speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	TestTrue(TEXT("Partial stick input is normalized"), FMath::IsNearlyEqual(Pawn->ConsumeMovementInputVector().Size(), 1.f));
	PC->RecordHeldInput(EKeys::Gamepad_LeftThumbstick, 0.f, User); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestEqual(TEXT("Releasing L3 restores walk speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.f);
	PC->RecordHeldInput(EKeys::Gamepad_LeftX, 0.f, User); Pawn->ConsumeMovementInputVector(); static_cast<AActor*>(Pawn)->Tick(.016f);
	TestTrue(TEXT("Centered stick stops"), Pawn->ConsumeMovementInputVector().IsNearlyZero());
	// Accepted locomotion continues through any Freeze blend, without accepting
	// a new direction/sprint command or borrowing the camera's changing yaw.
	PC->SetControlRotation(FRotator::ZeroRotator);
	PC->RecordHeldInput(EKeys::W, 1.f, User);
	PC->RecordHeldInput(EKeys::Gamepad_LeftThumbstick, 1.f, User);
	static_cast<AActor*>(Pawn)->Tick(.016f);
	const FVector AcceptedDirection = Pawn->ConsumeMovementInputVector();
	auto* Participant = Pawn->FindComponentByClass<UTAFreezeComponent>();
	auto* FreezeRegistry = World->GetSubsystem<UTAFreezeSubsystem>();
	FreezeRegistry->RegisterParticipant(Participant);
	const auto FreezeInputHandle = PC->AcquireInputRequest(ScanRequest);
	PC->RecordHeldInput(EKeys::W, 0.f, User);
	PC->RecordHeldInput(EKeys::D, 1.f, User);
	PC->RecordHeldInput(EKeys::Gamepad_LeftThumbstick, 0.f, User);
	PC->SetControlRotation(FRotator(0, 90, 0));
	for (float Strength : {0.f, 0.f, .1f, .5f, .9f, .5f, .1f, 0.f})
	{
		FreezeRegistry->RequestFreeze(PC, Strength);
		TestTrue(TEXT("Freeze request remains live at the zero endpoint"), Participant->HasFreezeRequest());
		TestFalse(TEXT("Move is prohibited throughout the command preservation test"), PC->AllowsInput(ETAInputCapability::Move));
		static_cast<AActor*>(Pawn)->Tick(.016f);
		TestTrue(TEXT("Freeze advances the accepted world direction"), Pawn->ConsumeMovementInputVector().Equals(AcceptedDirection));
		TestEqual(TEXT("Denied sprint changes do not rewrite command speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.f);
	}
	FreezeRegistry->RequestFreeze(PC, 1.f);
	static_cast<AActor*>(Pawn)->Tick(.016f);
	TestTrue(TEXT("Full Freeze stops command advancement"), Pawn->ConsumeMovementInputVector().IsNearlyZero());
	FreezeRegistry->ReleaseFreeze(PC);
	TestFalse(TEXT("Explicit release ends Freeze request"), Participant->HasFreezeRequest());
	static_cast<AActor*>(Pawn)->Tick(.016f);
	TestTrue(TEXT("Denied Move without Freeze cannot keep submitting old command"), Pawn->ConsumeMovementInputVector().IsNearlyZero());
	PC->ReleaseInputRequest(FreezeInputHandle);
	static_cast<AActor*>(Pawn)->Tick(.016f);
	TestTrue(TEXT("Thaw accepts the currently held direction"), Pawn->ConsumeMovementInputVector().Equals(FVector(-1, 0, 0), .001f));
	TestEqual(TEXT("Thaw accepts current sprint release"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.f);
	PC->RecordHeldInput(EKeys::D, 0.f, User);
	PC->RecordHeldInput(EKeys::W, 1.f, User);
	Mappings.RemoveMappingContext(Context, Options);
	TestFalse(TEXT("Removed mappings disable held actions"), PC->ReadHeldAction(Forward).IsNonZero());
	Mappings.AddMappingContext(Context, 0, Options);
	TestTrue(TEXT("Restoring mappings reads still held key"), PC->ReadHeldAction(Forward).IsNonZero());
	PC->NotifyApplicationActivationChanged(false);
	PC->NotifyApplicationActivationChanged(true);
	TestFalse(TEXT("Focus loss clears stale keys"), PC->ReadHeldAction(Forward).IsNonZero());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHeldParkourTest, "TheAwakening.Input.HeldParkour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHeldParkourTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACharacter* Pawn = World->SpawnActor<ACharacter>();
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	UTAParkourComponent* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	if (!TestNotNull(TEXT("Parkour input controller"), Held.PC)) { World->DestroyWorld(false); return false; }
	Held.Update(true, false);
	TestFalse(TEXT("Holding before marker waits"), Parkour->IsParkouring());
	ATAParkourMarker* Marker = World->SpawnActor<ATAParkourMarker>();
	Parkour->RegisterMarker(Marker);
	Held.Update(true, false);
	TestTrue(TEXT("Entering while held starts parkour"), Parkour->IsParkouring());
	Parkour->TickComponent(1.f, LEVELTICK_All, nullptr);
	Held.Update(true, false);
	TestFalse(TEXT("Same overlap does not loop"), Parkour->IsParkouring());
	Held.Update(false, false); Held.Update(true, false);
	TestTrue(TEXT("Release rearms same marker"), Parkour->IsParkouring());
	Parkour->TickComponent(1.f, LEVELTICK_All, nullptr);
	Parkour->UnregisterMarker(Marker); Parkour->RegisterMarker(Marker);
	Held.Update(true, false);
	TestTrue(TEXT("Real exit and reentry rearms a held request"), Parkour->IsParkouring());
	Parkour->TickComponent(1.f, LEVELTICK_All, nullptr);
	Held.Update(false, false);
	UTAFreezeComponent* Freeze = NewObject<UTAFreezeComponent>(Pawn);
	Pawn->AddInstanceComponent(Freeze); Freeze->RegisterComponent();
	UTAFreezeSubsystem* Registry = World->GetSubsystem<UTAFreezeSubsystem>();
	Registry->RegisterParticipant(Freeze); Registry->RequestFreeze(Marker);
	Held.Update(true, false);
	TestFalse(TEXT("Frozen actor cannot start through component API"), Parkour->IsParkouring());
	Registry->ReleaseFreeze(Marker); Held.Update(true, false);
	TestTrue(TEXT("Still held request starts after thaw"), Parkour->IsParkouring());
	Parkour->TickComponent(1.f, LEVELTICK_All, nullptr);
	ATAParkourMarker* Next = World->SpawnActor<ATAParkourMarker>();
	Next->MarkerType = ETAParkourMarkerType::DropDown;
	Parkour->RegisterMarker(Next); Held.Update(false, true);
	TestTrue(TEXT("Held drop starts at another marker"), Parkour->IsParkouring());
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAParkourFacingTest, "TheAwakening.Input.ParkourFacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAParkourFacingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACharacter* Pawn = World->SpawnActor<ACharacter>();
	UCharacterMovementComponent* Move = Pawn->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Walking);
	Move->bOrientRotationToMovement = true;
	UTAParkourComponent* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	if (!TestNotNull(TEXT("Parkour input controller"), Held.PC)) { World->DestroyWorld(false); return false; }
	ATAParkourMarker* Marker = World->SpawnActor<ATAParkourMarker>();
	// Exercise the real overlap handler, including the landing preview owner.
	struct FOverlapArgs
	{
		UPrimitiveComponent* Component = nullptr;
		AActor* OtherActor = nullptr;
		UPrimitiveComponent* OtherComponent = nullptr;
		int32 BodyIndex = 0;
		bool bFromSweep = false;
		FHitResult Sweep;
	} Args;
	Args.OtherActor = Pawn;
	{
		// This isolated world has no initialized gameplay actors; allow its native overlap event.
		TGuardValue<bool> AllowEvents(GAllowActorScriptExecutionInEditor, true);
		Marker->ProcessEvent(Marker->FindFunctionChecked(TEXT("OnBeginOverlap")), &Args);
	}
	Pawn->SetActorRotation(FRotator(0, -90, 0));
	Move->Velocity = FVector(0, -500, 0);
	TestFalse(TEXT("Moving backward cannot parkour"), Parkour->CanParkourToMarker(Marker));
	static_cast<AActor*>(Marker)->Tick(.016f);
	TestTrue(TEXT("Unavailable landing preview hidden"), Marker->GetPromptWidgetComponent()->bHiddenInGame);
	for (float Yaw : {0.f, 90.f, 180.f})
	{
		Pawn->SetActorRotation(FRotator(0, Yaw, 0));
		TestTrue(TEXT("Forward hemisphere includes both boundaries"), Parkour->CanParkourToMarker(Marker));
	}
	Pawn->SetActorRotation(FRotator(0, -90, 0)); Move->Velocity = FVector::ZeroVector;
	TestTrue(TEXT("Stationary exception enabled by default"), Parkour->CanParkourToMarker(Marker));
	Parkour->bAllowStationaryBackFacing = false;
	TestFalse(TEXT("Stationary exception can be disabled"), Parkour->CanParkourToMarker(Marker));
	Parkour->bAllowStationaryBackFacing = true;
	static_cast<AActor*>(Marker)->Tick(.016f);
	TestFalse(TEXT("Stationary exception also allows preview"), Marker->GetPromptWidgetComponent()->bHiddenInGame);
	TestTrue(TEXT("Landing anchor stays hidden when preview allowed"), Marker->LandingTargetComponent->bHiddenInGame);
	if (AActor* Anchor = Marker->LandingTargetComponent->GetChildActor())
		TestTrue(TEXT("Landing child actor stays hidden"), Anchor->IsHidden());
	Held.Update(true, false);
	TestTrue(TEXT("Stationary backward launch starts"), Parkour->IsParkouring());
	TestTrue(TEXT("Launch faces landing direction"), Pawn->GetActorForwardVector().Equals(FVector(0, 1, 0), .001f));
	TestFalse(TEXT("Movement auto rotation suspended"), Move->bOrientRotationToMovement);
	static_cast<AActor*>(Marker)->Tick(.016f);
	TestTrue(TEXT("Preview hidden in flight"), Marker->GetPromptWidgetComponent()->bHiddenInGame);
	Pawn->SetActorRotation(FRotator(0, -90, 0));
	Parkour->TickComponent(.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Flight keeps launch facing"), Pawn->GetActorForwardVector().Equals(FVector(0, 1, 0), .001f));
	Parkour->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Original movement rotation restored"), Move->bOrientRotationToMovement);
	TestTrue(TEXT("Animation facing stays stable at endpoint"), Parkour->GetParkourFacing());
	TestFalse(TEXT("Consumed marker unavailable to preview and execution"), Parkour->CanParkourToMarker(Marker));
	Held.Update(false, false);
	Pawn->SetActorLocation(FVector::ZeroVector);
	Parkour->bAllowStationaryBackFacing = false;
	Marker->LandingSpline->SetLocationAtSplinePoint(0, FVector(-100, 0, -200), ESplineCoordinateSpace::Local);
	Marker->LandingSpline->SetLocationAtSplinePoint(1, FVector(100, 0, -200), ESplineCoordinateSpace::Local);
	Pawn->SetActorRotation(FRotator(0, -90, 0));
	TestTrue(TEXT("Vertical route has no horizontal restriction"), Parkour->CanParkourToMarker(Marker));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAParkourMomentumTest, "TheAwakening.Input.ParkourMomentum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAParkourMomentumTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACharacter* Pawn = World->SpawnActor<ACharacter>();
	UTAMovementComponent* Move = NewObject<UTAMovementComponent>(Pawn);
	Pawn->AddInstanceComponent(Move); Move->RegisterComponent();
	Move->SetUpdatedComponent(Pawn->GetRootComponent());
	Move->SetMovementMode(MOVE_Walking);
	Move->MaxWalkSpeed = 500.f;
	auto SetAcceleration = [Move](float Y)
	{
		*FindFProperty<FStructProperty>(Move->GetClass(), TEXT("Acceleration"))->ContainerPtrToValuePtr<FVector>(Move) = FVector(0, Y, 0);
		FindFProperty<FFloatProperty>(Move->GetClass(), TEXT("AnalogInputModifier"))->SetPropertyValue_InContainer(Move, Y == 0.f ? 0.f : 1.f);
	};
	Move->BeginParkourLanding(FVector(0, 750, -300));
	TestEqual(TEXT("Landing drops vertical momentum"), Move->Velocity.Z, 0.0);
	SetAcceleration(0);
	Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Release coasts forward first"), Move->Velocity.Y > 0.f && Move->Velocity.Y < 750.f);
	for (int i = 0; i < 30; ++i) Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Release brakes to rest"), Move->Velocity.IsNearlyZero());
	Move->BeginParkourLanding(FVector(0, 750, 0)); SetAcceleration(4000);
	Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Sprint release retains above-walk momentum initially"), Move->Velocity.Y > 500.f && Move->Velocity.Y < 750.f);
	for (int i = 0; i < 30; ++i) Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Sprint release settles at walk speed"), FMath::IsNearlyEqual(Move->Velocity.Y, 500.0, .1));
	Move->BeginParkourLanding(FVector(0, 500, 0)); SetAcceleration(-4000);
	Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Reverse initially still moves forward"), Move->Velocity.Y > 0.f);
	for (int i = 0; i < 30; ++i) Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Reverse settles at opposite walk speed"), FMath::IsNearlyEqual(Move->Velocity.Y, -500.0, .1));
	for (int i = 0; i < 10; ++i) Move->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestFalse(TEXT("Landing response expires"), Move->IsParkourLanding());
	// Verify the real parkour lifecycle captures before clearing and restores at completion.
	UTAParkourComponent* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	if (!TestNotNull(TEXT("Parkour input controller"), Held.PC)) { World->DestroyWorld(false); return false; }
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Pawn->SetActorRotation(FRotator(0, 90, 0));
	Pawn->GetCharacterMovement()->Velocity = FVector(0, 750, -200);
	ATAParkourMarker* Marker = World->SpawnActor<ATAParkourMarker>();
	Parkour->RegisterMarker(Marker); Held.Update(true, false);
	TestTrue(TEXT("Momentum test launches parkour"), Parkour->IsParkouring());
	Held.Update(false, false);
	Parkour->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Parkour completion restores takeoff horizontal velocity despite release"),
		Pawn->GetCharacterMovement()->Velocity.Equals(FVector(0, 750, 0), .1));
	// The empty world has no safe ground: inherited momentum must not bypass the ledge check.
	UClass* GamePawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	AThe_AwakeningCharacter* GamePawn = GamePawnClass ? World->SpawnActor<AThe_AwakeningCharacter>(GamePawnClass) : nullptr;
	if (!TestNotNull(TEXT("Concrete character for ledge test"), GamePawn)) { World->DestroyWorld(false); return false; }
	UTAMovementComponent* GameMove = CastChecked<UTAMovementComponent>(GamePawn->GetCharacterMovement());
	GameMove->SetMovementMode(MOVE_Walking);
	GameMove->BeginParkourLanding(FVector(0, 750, 0));
	GameMove->CalcVelocity(.016f, 8.f, false, 2000.f);
	TestTrue(TEXT("Unsafe landing momentum stops at ledge"), GameMove->Velocity.IsNearlyZero());
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPuzzleScopeTest, "TheAwakening.Puzzle.Scope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPuzzleScopeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* ControllerClass = LoadClass<AThe_AwakeningPlayerController>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = ControllerClass ? World->SpawnActor<AThe_AwakeningPlayerController>(ControllerClass) : nullptr;
	if (!TestNotNull(TEXT("Scope controller"), PC)) { World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	World->AddController(PC);
	UClass* WidgetClass = LoadClass<UTAPathPuzzleWidget>(nullptr, TEXT("/Game/UI/Minigame/WBP_TestPuzzle.WBP_TestPuzzle_C"));
	if (!TestNotNull(TEXT("Scope WBP"), WidgetClass)) { World->DestroyWorld(false); return false; }
	for (ETAPuzzleDifficulty Difficulty : {ETAPuzzleDifficulty::Easy, ETAPuzzleDifficulty::Medium, ETAPuzzleDifficulty::Hard})
	{
		auto* Widget = CreateWidget<UTAPathPuzzleWidget>(PC, WidgetClass);
		Widget->PuzzleSeed = 12345; Widget->PuzzleSettings.Difficulty = Difficulty;
		Widget->PuzzleSettings.bEnableMainGameEffects = false;
		TestTrue(TEXT("Scope starts all difficulty levels"), Widget->StartPuzzle());
		if (!Widget->Session) continue;
		Widget->NativeConstruct();
		TestFalse(TEXT("Scope hides cursor"), PC->bShowMouseCursor);
		const int32 User = Local->GetControllerId();
		for (const auto Key : {EKeys::W, EKeys::S, EKeys::A, EKeys::D})
		{
			PC->RecordHeldInput(Key, 1.f, User);
			const FVector2D Expected = Key == EKeys::W ? FVector2D(0,1) : Key == EKeys::S ? FVector2D(0,-1) : Key == EKeys::A ? FVector2D(1,0) : FVector2D(-1,0);
			TestTrue(TEXT("WASD moves board in specified inverse direction"), Widget->GetPlayerPanInput().Equals(Expected));
			PC->RecordHeldInput(Key, 0.f, User);
		}
		PC->RecordHeldInput(EKeys::Gamepad_LeftX, 1.f, User);
		TestTrue(TEXT("LS right moves board left"), Widget->GetPlayerPanInput().Equals(FVector2D(-1,0)));
		PC->RecordHeldInput(EKeys::Gamepad_LeftX, 0.f, User);
		const FVector2D Start = Widget->GetBoardViewCenter();
		TestTrue(TEXT("Repeat confirmation consumed"), PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Bottom, true, User));
		TestEqual(TEXT("Repeat does not select"), Widget->Session->Progress.Path.Num(), 0);
		FTAInputRequest Cover;
		Cover.Owner = PC; Cover.Priority = 400;
		Cover.Allowed = {ETAInputCapability::Confirm, ETAInputCapability::Undo, ETAInputCapability::Pan};
		const auto CoverHandle = PC->AcquireInputRequest(Cover);
		Widget->ExecutePlayerInput(EKeys::LeftMouseButton, ETAInputCapability::Confirm);
		TestEqual(TEXT("Covered puzzle cannot borrow another winner's Confirm"), Widget->Session->Progress.Path.Num(), 0);
		PC->ReleaseInputRequest(CoverHandle);
		PC->RoutePlayerInputKey(EKeys::LeftMouseButton, false, User);
		TestEqual(TEXT("Reticle confirms initial start node"), Widget->Session->Progress.Path.Num(), 1);
		TestTrue(TEXT("Selection rebuild keeps view center"), Widget->GetBoardViewCenter().Equals(Start));
		const float Time = Widget->Session->TimeRemaining;
		Widget->PanBoard(FVector2D(-1,0), .1f);
		TestTrue(TEXT("Board left moves view right"), Widget->GetBoardViewCenter().X < Start.X);
		const auto Panned = Widget->GetBoardViewCenter(); Widget->RefreshBoard();
		TestTrue(TEXT("Refresh preserves panning"), Widget->GetBoardViewCenter().Equals(Panned));
		TestEqual(TEXT("Panning does not alter timer"), Widget->Session->TimeRemaining, Time);
		Widget->Session->AdvanceTime(.5f);
		TestTrue(TEXT("Timer remains active"), Widget->Session->TimeRemaining < Time);
		Widget->PanBoard(FVector2D(-1,-1), 1000.f);
		TestTrue(TEXT("Board movement bounded"), Widget->GetBoardViewCenter().X <= Widget->Appearance.BoardSize.X && Widget->GetBoardViewCenter().Y <= Widget->Appearance.BoardSize.Y);
		Widget->RemoveFromParent();
		Widget->NativeDestruct();
		TestTrue(TEXT("Scope input released"), Widget->GetPlayerPanInput().IsNearlyZero());
	}
	World->DestroyWorld(false); return true;
}
#endif
