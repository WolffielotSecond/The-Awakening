#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAFreezeBlendTest, "TheAwakening.Freeze.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAFreezeBlendTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UTAFreezeSubsystem* Registry = World->GetSubsystem<UTAFreezeSubsystem>();
	AActor* Actor = World->SpawnActor<AActor>();
	Actor->CustomTimeDilation = .8f;
	UTAFreezeComponent* Freeze = NewObject<UTAFreezeComponent>(Actor);
	Freeze->RegisterComponent();
	Registry->RegisterParticipant(Freeze);
	UActorComponent* Movement = NewObject<USceneComponent>(Actor);
	Movement->PrimaryComponentTick.bCanEverTick = true;
	Movement->RegisterComponent();
	Movement->SetComponentTickEnabled(true);
	UObject* Scan = Movement;
	UObject* Puzzle = Actor;

	Registry->RequestFreeze(Scan, 0.f);
	TestTrue(TEXT("Zero-strength request is active for participants"), Freeze->HasFreezeRequest());
	TestEqual(TEXT("Zero strength preserves original dilation"), Actor->CustomTimeDilation, .8f);
	TestFalse(TEXT("Zero-strength request does not suspend ticks"), Freeze->IsFrozen());
	Registry->RequestFreeze(Scan, .5f);
	TestEqual(TEXT("Blend scales original actor dilation"), Actor->CustomTimeDilation, .4f);
	TestFalse(TEXT("Transition does not suspend ticks"), Freeze->IsFrozen());
	TestTrue(TEXT("Moving components remain enabled during transition"), Movement->IsComponentTickEnabled());
	float ComponentDelta = -1.f;
	FActorComponentTickFunction::ExecuteTickHelper(Movement, false, 1.f, LEVELTICK_All,
		[&](float Delta) { ComponentDelta = Delta; });
	TestEqual(TEXT("Engine component ticks inherit blended delta"), ComponentDelta, .4f);

	Registry->RequestFreeze(Puzzle);
	TestEqual(TEXT("Puzzle overrides partial scan freeze"), Actor->CustomTimeDilation, 0.f);
	TestTrue(TEXT("Full endpoint freezes"), Freeze->IsFrozen());
	TestFalse(TEXT("Full endpoint disables gameplay components"), Movement->IsComponentTickEnabled());
	Registry->RequestFreeze(Scan, .25f);
	TestTrue(TEXT("Scan fade cannot release puzzle freeze"), Freeze->IsFrozen());
	Registry->ReleaseFreeze(Puzzle);
	TestTrue(TEXT("Returning to partial freeze restores component ticks"), Movement->IsComponentTickEnabled());
	TestTrue(TEXT("Resumes at current scan strength"), FMath::IsNearlyEqual(Actor->CustomTimeDilation, .6f));
	Registry->RequestFreeze(Scan, .75f);
	TestTrue(TEXT("Reversing direction preserves original baseline"), FMath::IsNearlyEqual(Actor->CustomTimeDilation, .2f));
	Registry->ReleaseFreeze(Scan);
	TestFalse(TEXT("Last explicit release ends request lifetime"), Freeze->HasFreezeRequest());
	TestEqual(TEXT("Final release restores prior dilation, not one"), Actor->CustomTimeDilation, .8f);
	Registry->RequestFreeze(Scan, 2.f);
	TestTrue(TEXT("Curve overshoot clamps to full freeze"), Freeze->IsFrozen());
	Registry->RequestFreeze(Scan, -1.f);
	TestFalse(TEXT("Zero strength releases freeze"), Registry->IsFrozen());
	TestEqual(TEXT("Negative curve value restores baseline"), Actor->CustomTimeDilation, .8f);
	TestTrue(TEXT("Clamped strength zero is not request release"), Freeze->HasFreezeRequest());
	Registry->UnregisterParticipant(Freeze);
	TestFalse(TEXT("Unregistered component cannot retain request participation"), Freeze->HasFreezeRequest());
	Registry->RegisterParticipant(Freeze);
	Registry->ReleaseFreeze(Scan);
	UObject* ExpiringSource = NewObject<USceneComponent>();
	Registry->RequestFreeze(ExpiringSource, 0.f);
	TestTrue(TEXT("Live source activates zero-strength request"), Freeze->HasFreezeRequest());
	ExpiringSource->MarkAsGarbage();
	TestFalse(TEXT("Invalid source is not an active request before pruning"), Freeze->HasFreezeRequest());
	Registry->ReleaseFreeze(ExpiringSource);
	World->DestroyWorld(false);
	return true;
}
#endif
