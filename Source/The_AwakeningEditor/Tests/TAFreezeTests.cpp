#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAFreezeLifecycleTest, "TheAwakening.Freeze.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAFreezeLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UTAFreezeSubsystem* Registry = World->GetSubsystem<UTAFreezeSubsystem>();
	if (!TestNotNull(TEXT("World freeze registry exists"), Registry))
	{
		World->DestroyWorld(false);
		return false;
	}
	AActor* Actor = World->SpawnActor<AActor>();
	Actor->PrimaryActorTick.bCanEverTick = true;
	Actor->SetActorTickEnabled(true);
	auto AddTickComponent = [Actor](bool bEnabled, bool bExempt)
	{
		UActorComponent* Component = NewObject<USceneComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->PrimaryComponentTick.bCanEverTick = true;
		Component->RegisterComponent();
		Component->SetComponentTickEnabled(bEnabled);
		if (bExempt) Component->ComponentTags.Add(TEXT("FreezeExempt"));
		return Component;
	};
	UActorComponent* Moving = AddTickComponent(true, false);
	UActorComponent* AlreadyDisabled = AddTickComponent(false, false);
	UActorComponent* Exempt = AddTickComponent(true, true);
	UTAFreezeComponent* Freeze = NewObject<UTAFreezeComponent>(Actor);
	Actor->AddInstanceComponent(Freeze);
	Freeze->RegisterComponent();
	Registry->RegisterParticipant(Freeze);
	UObject* ScanSource = Moving;
	UObject* OtherSource = Exempt;
	Registry->RequestFreeze(ScanSource);
	Registry->RequestFreeze(ScanSource); // Repeated requests must not overwrite the snapshot.
	TestTrue(TEXT("Actor reports frozen"), UTAFreezeComponent::IsActorFrozen(Actor));
	TestFalse(TEXT("Actor tick stopped"), Actor->IsActorTickEnabled());
	TestFalse(TEXT("Movement/animation tick stopped"), Moving->IsComponentTickEnabled());
	TestTrue(TEXT("Exempt effects continue ticking"), Exempt->IsComponentTickEnabled());
	Registry->RequestFreeze(OtherSource);
	Registry->ReleaseFreeze(ScanSource);
	TestTrue(TEXT("Another source still holds freeze"), Freeze->IsFrozen());

	AActor* LateActor = World->SpawnActor<AActor>();
	UTAFreezeComponent* LateFreeze = NewObject<UTAFreezeComponent>(LateActor);
	LateActor->AddInstanceComponent(LateFreeze);
	LateFreeze->RegisterComponent();
	Registry->RegisterParticipant(LateFreeze);
	TestTrue(TEXT("Late participant freezes immediately"), LateFreeze->IsFrozen());
	Registry->ReleaseFreeze(OtherSource);
	TestFalse(TEXT("Last release unfreezes actor"), Freeze->IsFrozen());
	TestFalse(TEXT("Late participant also restored"), LateFreeze->IsFrozen());
	TestTrue(TEXT("Original actor tick restored"), Actor->IsActorTickEnabled());
	TestTrue(TEXT("Original component tick restored"), Moving->IsComponentTickEnabled());
	TestFalse(TEXT("Previously disabled component stays disabled"), AlreadyDisabled->IsComponentTickEnabled());
	Registry->ReleaseFreeze(ScanSource);
	TestTrue(TEXT("Repeated release preserves restored state"), Moving->IsComponentTickEnabled());
	World->DestroyWorld(false);
	return true;
}
#endif
