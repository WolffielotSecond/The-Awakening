#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningPlayerController.h"
#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "Story/TADialogueWidget.h"
#include "Story/TADialogueHistoryWidget.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "Scan/TAScanningComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "Core/TAFreezeComponent.h"
#include "GameFramework/Pawn.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInputLifecycleTest, "TheAwakening.Input.OwnershipLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInputLifecycleTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC)) { World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	World->AddController(PC); // UMG resolves owning players through the world controller list.
	PC->SynchronizeInputPresentation();
	TestTrue(TEXT("Deterministic base"), PC->GetInputWinner().IsBase() && !PC->bShowMouseCursor && PC->GetInputWinner().Request.Presentation.InputMode == ETAInputModeRequirement::GameOnly);
	int32 Changes = 0;
	const auto Listener = PC->OnInputOwnerChanged.AddLambda([&Changes]() { ++Changes; });
	auto* Inventory = CreateWidget<UTAInventoryPanelWidget>(PC, UTAInventoryPanelWidget::StaticClass());
	auto* Dialogue = CreateWidget<UTADialogueWidget>(PC, UTADialogueWidget::StaticClass());
	auto* History = CreateWidget<UTADialogueHistoryWidget>(PC, UTADialogueHistoryWidget::StaticClass());
	if (!TestNotNull(TEXT("Inventory"), Inventory) || !TestNotNull(TEXT("Dialogue"), Dialogue) || !TestNotNull(TEXT("History"), History))
	{ PC->OnInputOwnerChanged.Remove(Listener); World->DestroyWorld(false); return false; }
	// Exercise actual lifecycle hooks without requiring a PIE viewport in automation.
 // Native fixtures do not inherit WBP action defaults. Supply the project's
 // shared actions before Construct so validation exercises a configured menu.
 auto AssignAction=[](UObject* Widget,const TCHAR* Property,const TCHAR* Path)
 {
  if (auto* Field=FindFProperty<FObjectPropertyBase>(Widget->GetClass(),Property))
   Field->SetObjectPropertyValue_InContainer(Widget,LoadObject<UInputAction>(nullptr,Path));
 };
 AssignAction(Inventory,TEXT("PreviousPageAction"),TEXT("/Game/Input/Actions/Inventory/IA_Inventory_PreviousPage"));
 AssignAction(Inventory,TEXT("NextPageAction"),TEXT("/Game/Input/Actions/Inventory/IA_Inventory_NextPage"));
 AssignAction(Inventory,TEXT("ConfirmAction"),TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Confirm"));
 AssignAction(Inventory,TEXT("GamepadDragModeAction"),TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Drag"));
 AssignAction(Dialogue,TEXT("ChoicePreviousAction"),TEXT("/Game/Input/Actions/IA_ChoicePrevious"));
 AssignAction(Dialogue,TEXT("ChoiceNextAction"),TEXT("/Game/Input/Actions/IA_ChoiceNext"));
 AssignAction(Dialogue,TEXT("ChoiceConfirmAction"),TEXT("/Game/Input/Actions/IA_ChoiceConfirm"));
	Inventory->NativeConstruct();
	const auto InventoryHandle = PC->GetInputWinner().Handle;
	TestTrue(TEXT("Inventory owns its request"), PC->GetInputWinner().Request.Owner.Get() == Inventory && PC->bShowMouseCursor);
	Dialogue->NativeConstruct();
	const auto DialogueHandle = PC->GetInputWinner().Handle;
	TestTrue(TEXT("Dialogue replaces equal priority owner"), DialogueHandle != InventoryHandle && PC->GetInputWinner().Request.Owner.Get() == Dialogue);
	History->NativeConstruct();
	const auto HistoryHandle = PC->GetInputWinner().Handle;
	TestTrue(TEXT("History has independent ownership"), PC->GetInputWinner().Request.Owner.Get() == History);
	PC->NotifyApplicationActivationChanged(false);
	PC->RefreshInputOwnership();
	TestFalse(TEXT("Owner authorization also requires external ownership"), PC->AllowsInputFor(HistoryHandle, History, ETAInputCapability::Close));
	TestTrue(TEXT("Application loss preserves persistent history winner"), PC->GetInputWinner().Handle == HistoryHandle);
	PC->NotifyApplicationActivationChanged(true);
	TestTrue(TEXT("Winning history regains close authorization"), PC->AllowsInputFor(HistoryHandle, History, ETAInputCapability::Close));
	Inventory->RemoveFromParent();
	Inventory->NativeDestruct();
	TestTrue(TEXT("Out of order close/destruct cannot release history"), PC->GetInputWinner().Handle == HistoryHandle);
	const int32 BeforeRepeat = Changes;
	PC->ReleaseInputRequest(InventoryHandle);
	TestEqual(TEXT("Stale release has no ownership side effect"), Changes, BeforeRepeat);
	History->RemoveFromParent();
	History->NativeDestruct();
	TestTrue(TEXT("History close restores remaining dialogue"), PC->GetInputWinner().Handle == DialogueHandle);
	Dialogue->RemoveFromParent();
	Dialogue->NativeDestruct();
	TestTrue(TEXT("Final close restores declared base"), PC->GetInputWinner().IsBase() && !PC->bShowMouseCursor);

	auto* Owner = NewObject<UInputAction>();
	auto* Focus = NewObject<UButton>();
	FTAInputRequest Request;
	Request.Owner = Owner; Request.Priority = 400;
	Request.Allowed = {ETAInputCapability::Navigate};
	Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
	Request.Presentation.bShowCursor = true;
	Request.Presentation.Focus = ETAInputFocusRequirement::Target;
	Request.Presentation.FocusTarget = Focus;
	const auto Handle = PC->AcquireInputRequest(Request);
	const int32 BeforeFocusExpiry = Changes;
	Focus->MarkAsGarbage(); PC->RefreshInputOwnership();
	TestTrue(TEXT("Dead target retains request and cursor policy"), PC->GetInputWinner().Handle == Handle && PC->bShowMouseCursor);
	TestEqual(TEXT("Focus expiry is not ownership change"), Changes, BeforeFocusExpiry);
	Owner->MarkAsGarbage(); PC->RefreshInputOwnership();
	TestTrue(TEXT("Owner expiry applies base without explicit release"), PC->GetInputWinner().IsBase() && !PC->bShowMouseCursor);
	const int32 AfterExpiry = Changes;
	PC->ReleaseInputRequest(Handle); PC->ReleaseInputRequest(0);
	TestEqual(TEXT("Expired/zero release has no ownership side effect"), Changes, AfterExpiry);
	PC->OnInputOwnerChanged.Remove(Listener);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAExternalInputOwnershipTest, "TheAwakening.Input.ExternalOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAExternalInputOwnershipTest::RunTest(const FString&)
{
	using TAInputOwnershipAdapter::EState;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC)) { World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	World->AddController(PC);
	PC->Possess(World->SpawnActor<APawn>());
	int32 LostEvents = 0;
	const auto Listener = PC->OnPlayerInputOwnershipLost.AddLambda([&]()
	{
		++LostEvents;
		// Re-entrant observation must not broadcast again.
		PC->ObservePlayerInputOwnership(EState::Lost);
	});
	PC->ObservePlayerInputOwnership(EState::Lost);
	TestEqual(TEXT("Initial Lost is not a loss edge"), LostEvents, 0);
	PC->ObservePlayerInputOwnership(EState::Owned);
	PC->ObservePlayerInputOwnership(EState::Transition);
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestEqual(TEXT("Internal focus gap does not cancel"), LostEvents, 0);
	PC->ObservePlayerInputOwnership(EState::Transition);
	PC->ObservePlayerInputOwnership(EState::Lost);
	PC->ObservePlayerInputOwnership(EState::Lost);
	TestEqual(TEXT("Owned through gap to Lost emits once, including reentry"), LostEvents, 1);

	auto* Scan = PC->FindComponentByClass<UTAScanningComponent>();
	if (!TestNotNull(TEXT("Scan component"), Scan))
	{ PC->OnPlayerInputOwnershipLost.Remove(Listener); World->DestroyWorld(false); return false; }
	// Exercise the real component subscription/cancel/release hooks without a PIE viewport.
	auto* Freeze = World->GetSubsystem<UTAFreezeSubsystem>();
	auto* Participant = NewObject<UTAFreezeComponent>(PC->GetPawn());
	PC->GetPawn()->AddInstanceComponent(Participant); Participant->RegisterComponent();
	Freeze->RegisterParticipant(Participant);
	Scan->BeginPlay();
	TestTrue(TEXT("Scan starts"), Scan->StartScan());
	TestTrue(TEXT("Scan FadeIn owns a Freeze request before its first tick"), Participant->HasFreezeRequest());
	TestEqual(TEXT("Scan starts at zero Freeze strength"), Freeze->GetFreezeStrength(), 0.f);
	const auto ScanHandle = PC->GetInputWinner().Handle;
	Scan->UpdateScanTime(0.5f);
	Scan->UpdateTimeFreezeBlend();
	TestTrue(TEXT("Scan requested freeze"), Freeze && Freeze->GetFreezeStrength() > 0.f);
	PC->ObservePlayerInputOwnership(EState::Transition);
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestTrue(TEXT("Focus transition retains scan and exact request"), Scan->IsScanning() && PC->GetInputWinner().Handle == ScanHandle);
	PC->ObservePlayerInputOwnership(EState::Lost); // Adapter outcome for editor ownership; not an F8 simulation.
	TestFalse(TEXT("External loss ends active scanning/hover"), Scan->IsScanning());
	TestTrue(TEXT("External loss releases scan request"), PC->GetInputWinner().IsBase());
	TestTrue(TEXT("Scan Freeze request survives input release through FadeOut"), Participant->HasFreezeRequest());
	Scan->UpdateScanTime(2.f); Scan->UpdateTimeFreezeBlend();
	TestTrue(TEXT("Existing fade-out releases scan freeze"), Freeze && Freeze->GetFreezeStrength() == 0.f);
	TestFalse(TEXT("FadedOut releases the Freeze request, not just its strength"), Participant->HasFreezeRequest());
	PC->ObservePlayerInputOwnership(EState::Owned);
	TestFalse(TEXT("Recovery cannot restart held scan"), Scan->StartScan());
	Scan->NotifyScanInputReleased(false);
	TestTrue(TEXT("Release then new press starts scan"), Scan->StartScan());
	PC->NotifyApplicationActivationChanged(false);
	PC->NotifyApplicationActivationChanged(false);
	TestFalse(TEXT("Application loss uses same cancel lifecycle"), Scan->IsScanning());
	PC->NotifyApplicationActivationChanged(true);
	TestFalse(TEXT("Application activation is not physical release"), Scan->StartScan());
	Scan->NotifyScanInputReleased(false);
	TestTrue(TEXT("Scan can start again after observed release"), Scan->StartScan());

	auto* Puzzle = NewObject<UTAPathPuzzleWidget>(PC);
	FTAInputRequest Request; Request.Owner = Puzzle; Request.Priority = 300;
	Request.Allowed = {ETAInputCapability::Confirm, ETAInputCapability::Undo, ETAInputCapability::Pan};
	Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
	const int32 BeforeCover = LostEvents;
	const auto PuzzleHandle = PC->AcquireInputRequest(Request);
	TestFalse(TEXT("Puzzle winner independently cancels scan"), Scan->IsScanning());
	TestEqual(TEXT("Winner change is not external ownership loss"), LostEvents, BeforeCover);
	PC->NotifyApplicationActivationChanged(false);
	TestTrue(TEXT("Persistent puzzle request survives loss"), PC->GetInputWinner().Handle == PuzzleHandle);
	PC->NotifyApplicationActivationChanged(true);
	PC->ReleaseInputRequest(PuzzleHandle);
	TestTrue(TEXT("Puzzle close reveals base, not cancelled scan"), PC->GetInputWinner().IsBase());
	TestFalse(TEXT("Puzzle close does not revive held scan"), Scan->StartScan());
	Scan->NotifyScanInputReleased(false);
	Scan->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("Scan teardown releases its Freeze request"), Participant->HasFreezeRequest());
	TestFalse(TEXT("Component teardown unbinds external lifecycle"), PC->OnPlayerInputOwnershipLost.IsBoundToObject(Scan));
	PC->OnPlayerInputOwnershipLost.Remove(Listener);
	World->DestroyWorld(false);
	return true;
}
#endif
