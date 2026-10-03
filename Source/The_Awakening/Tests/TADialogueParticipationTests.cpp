#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningCharacter.h"
#include "Interaction/TADialogueParticipant.h"
#include "Interaction/TAInteractableActor.h"
#include "Story/ATAStoryTriggerActor.h"
#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "Movement/TAMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADialogueParticipationTest, "TheAwakening.Input.DialogueParticipation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADialogueParticipationTest::RunTest(const FString&)
{
	// This isolated world has not initialized actors; allow reflected interface dispatch.
	TGuardValue<bool> AllowEvents(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	auto* Pawn = PawnClass ? World->SpawnActor<AThe_AwakeningCharacter>(PawnClass) : nullptr;
	if (!TestNotNull(TEXT("Character"), Pawn)) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	auto* Trigger = World->SpawnActor<ATAStoryTriggerActor>();
	auto* Other = World->SpawnActor<ATAInteractableActor>();
	auto* Parkour = Pawn->FindComponentByClass<UTAParkourComponent>();
	auto* Move = Cast<UTAMovementComponent>(Pawn->GetCharacterMovement());
	Parkour->BeginPlay(); Move->SetMovementMode(MOVE_Walking);
	TestTrue(TEXT("Idle can participate"), ITADialogueParticipant::Execute_CanParticipateInDialogue(Pawn));
	Move->Velocity = FVector(500, 0, 0);
	TestTrue(TEXT("Normal walk can interact with dialogue"), ITAInteractable::Execute_CanInteract(Trigger, Pawn));
	Pawn->StopCurrentMovement();
	TestTrue(TEXT("Explicit stop stops walk"), Move->Velocity.IsNearlyZero());
	auto* Marker = World->SpawnActor<ATAParkourMarker>();
	Parkour->RegisterMarker(Marker); Parkour->UpdateHeldRequests(true, false);
	TestTrue(TEXT("Active parkour established"), Parkour->IsParkouring());
	TestFalse(TEXT("Active parkour cannot participate"), ITADialogueParticipant::Execute_CanParticipateInDialogue(Pawn));
	TestFalse(TEXT("Story prompt eligibility denies active parkour"), ITAInteractable::Execute_CanInteract(Trigger, Pawn));
	// StoryAsset is deliberately unset: execution must return at the shared eligibility
	// check, before the missing-asset warning or dialogue startup.
	ITAInteractable::Execute_OnInteract(Trigger, Pawn);
	TestTrue(TEXT("Other interaction types remain eligible"), ITAInteractable::Execute_CanInteract(Other, Pawn));
	Parkour->TickComponent(2.f, LEVELTICK_All, nullptr);
	Move->BeginParkourLanding(FVector(500, 0, 0));
	TestTrue(TEXT("Landing inertia permits dialogue again"), ITAInteractable::Execute_CanInteract(Trigger, Pawn));
	Pawn->AddMovementInput(FVector::ForwardVector, 1.f, true);
	Pawn->StopCurrentMovement();
	TestTrue(TEXT("Explicit stop clears velocity and pending input"), Move->Velocity.IsNearlyZero() && Pawn->GetPendingMovementInputVector().IsNearlyZero());
	TestFalse(TEXT("Explicit stop cancels landing inertia"), Move->IsParkourLanding());
	Pawn->StopCurrentMovement();
	TestTrue(TEXT("Stopping rest is harmless"), Move->Velocity.IsNearlyZero());
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
