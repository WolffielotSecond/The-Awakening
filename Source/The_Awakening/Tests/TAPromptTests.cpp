#include "UI/TAPromptComponent.h"
#include "Tests/TAParkourInputTestFixture.h"
#include "Tests/TAHealthTestListener.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "UI/TAPromptSettings.h"
#include "Interaction/TAInteractionQuery.h"
#include "Interaction/TAInteractableActor.h"
#include "Health/TACheckpoint.h"
#include "Inventory/TAWorldItem.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TANearbyPickupComponent.h"
#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "Core/TATimeSubsystem.h"
#include "Core/TAPlayerState.h"
#include "Core/TAPlayerInput.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TAPromptTests
{
	/** 项目的 PC / 角色蓝图；测试世界不跑 BeginPlay，需要的组件手动初始化。 */
	struct FFixture
	{
		UWorld* World = nullptr;
		AThe_AwakeningPlayerController* PC = nullptr;
		AThe_AwakeningCharacter* Pawn = nullptr;
		UTAPromptComponent* Prompt = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
			UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (!PCClass || !PawnClass) return;
			PC = World->SpawnActor<AThe_AwakeningPlayerController>(PCClass);
			Pawn = World->SpawnActor<AThe_AwakeningCharacter>(PawnClass, FVector::ZeroVector, FRotator::ZeroRotator);
			auto* Local = NewObject<ULocalPlayer>(GEngine);
			Local->PlayerController = PC; PC->Player = Local;
			PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
			if (PC->PlayerState) PC->PlayerState->Destroy();
			PC->PlayerState = World->SpawnActor<ATAPlayerState>();
			World->AddController(PC);
			PC->Possess(Pawn);
			Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Pawn->SetActorRotation(FRotator(0, 90, 0));
			Prompt = Pawn->FindComponentByClass<UTAPromptComponent>();
			if (Prompt)
			{
				Prompt->BeginPlay();
				Prompt->PromptRadius = 250.f;
			}
			if (UTAParkourComponent* Parkour = Pawn->GetParkourComponent())
			{
				static_cast<UActorComponent*>(Parkour)->BeginPlay();
			}
		}
		~FFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		bool Validate(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Controller BP"), PC) && Test.TestNotNull(TEXT("Character BP"), Pawn)
				&& Test.TestNotNull(TEXT("Prompt component"), Prompt);
		}

		/** 可交互物：给它一个阻挡 Visibility 的球体，让交互查询能发现它。 */
		ATAInteractableActor* SpawnInteractable(const FVector& Location)
		{
			auto* Actor = World->SpawnActor<ATAInteractableActor>(Location, FRotator::ZeroRotator);
			auto* Sphere = NewObject<USphereComponent>(Actor);
			Actor->AddInstanceComponent(Sphere);
			Sphere->SetupAttachment(Actor->GetRootComponent());
			Sphere->InitSphereRadius(30.f);
			Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Sphere->RegisterComponent();
			return Actor;
		}

		ATAParkourMarker* SpawnMarker(const FVector& Location, ETAParkourMarkerType Type)
		{
			auto* Marker = World->SpawnActorDeferred<ATAParkourMarker>(ATAParkourMarker::StaticClass(), FTransform(Location));
			Marker->MarkerType = Type;
			Marker->bEnableUpper = Type == ETAParkourMarkerType::JumpToPoint;
			Marker->bEnableLower = Type == ETAParkourMarkerType::DropDown;
			Marker->FinishSpawning(FTransform(Location));
			FTAParkourInputTestFixture::Register(Pawn->GetParkourComponent(), Marker);
			return Marker;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPromptInteractTest, "TheAwakening.Prompt.InteractMatchesExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPromptInteractTest::RunTest(const FString&)
{
	TAPromptTests::FFixture F;
	if (!F.Validate(*this)) return false;

	ATAInteractableActor* Far = F.SpawnInteractable(FVector(180.f, 0.f, 0.f));
	ATAInteractableActor* Near = F.SpawnInteractable(FVector(90.f, 0.f, 0.f));
	F.Prompt->UpdatePrompts();
	AActor* Shown = F.Prompt->GetShownSource(ETAPromptType::Interact);
	TestTrue(TEXT("Nearest interactable is prompted"), Shown == Near);
	TestTrue(TEXT("Prompted target is exactly what interaction would execute on"),
		Shown == FTAInteractionQuery::FindBestInteractable(F.Pawn, F.Prompt->PromptRadius));
	TestNotNull(TEXT("Anchor without a widget class falls back to the default prompt widget"),
		Near->GetPromptAnchor()->GetWidgetClass().Get());
	TestFalse(TEXT("Other interactable stays hidden"), Far->GetPromptAnchor()->IsPromptShown());

	// 冻结：执行被拒绝，提示同样隐藏；解冻后恢复
	UTAFreezeComponent* Freeze = F.Pawn->FindComponentByClass<UTAFreezeComponent>();
	UTAFreezeSubsystem* Registry = F.World->GetSubsystem<UTAFreezeSubsystem>();
	UTATimeSubsystem* Time = F.World->GetSubsystem<UTATimeSubsystem>();
	if (!TestNotNull(TEXT("Freeze participant"), Freeze) || !TestNotNull(TEXT("Time subsystem"), Time)) return false;
	Registry->RegisterParticipant(Freeze);
	UObject* FreezeSource = NewObject<UTAHealthTestListener>();
	Time->RequestFreeze(FreezeSource);
	F.Prompt->UpdatePrompts();
	TestNull(TEXT("Fully frozen player sees no prompts"), F.Prompt->GetShownSource(ETAPromptType::Interact));
	TestFalse(TEXT("Frozen: previously shown anchor hidden"), Near->GetPromptAnchor()->IsPromptShown());
	Time->ReleaseFreeze(FreezeSource);
	F.Prompt->UpdatePrompts();
	TestTrue(TEXT("Prompt returns after unfreeze"), F.Prompt->GetShownSource(ETAPromptType::Interact) == Near);

	// 附近有可拾取物品：交互键交给拾取列表，交互提示让出
	UTANearbyPickupComponent* Pickup = F.Pawn->FindComponentByClass<UTANearbyPickupComponent>();
	if (!TestNotNull(TEXT("Nearby pickup component"), Pickup)) return false;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATAWorldItem* Item = F.World->SpawnActor<ATAWorldItem>(FVector(40.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
	Item->SetupItem(NewObject<UTAItemDefinition>(), 1);
	Item->RegisterAllComponents();
	TestNull(TEXT("World items carry no prompt anchor"), UTAInteractionPromptAnchor::FindAnchor(Item, ETAPromptType::Interact));
	Pickup->RefreshNearbyItems();
	if (TestTrue(TEXT("Pickup list sees the item"), Pickup->HasItems()))
	{
		F.Prompt->UpdatePrompts();
		TestNull(TEXT("Interact prompt yields to the pickup list"), F.Prompt->GetShownSource(ETAPromptType::Interact));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPromptElementsTest, "TheAwakening.Prompt.Elements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPromptElementsTest::RunTest(const FString&)
{
	TAPromptTests::FFixture F;
	if (!F.Validate(*this)) return false;

	ATACheckpoint* Checkpoint = F.World->SpawnActor<ATACheckpoint>(FVector(5000.f, 0.f, 0.f), FRotator::ZeroRotator);
	UTAInteractionPromptAnchor* CheckpointAnchor = UTAInteractionPromptAnchor::FindAnchor(Checkpoint, ETAPromptType::Interact);
	if (!TestNotNull(TEXT("Checkpoint has an interact prompt anchor"), CheckpointAnchor)) return false;
	TestEqual(TEXT("Checkpoint prompt text"), CheckpointAnchor->ResolveTextId(), FString(TEXT("Interact_Checkpoint")));
	CheckpointAnchor->TextId = TEXT("Custom_Key");
	TestEqual(TEXT("Anchor text id overrides the element"), CheckpointAnchor->ResolveTextId(), FString(TEXT("Custom_Key")));

	ATAParkourMarker* JumpFar = F.SpawnMarker(FVector(150.f, 0.f, 0.f), ETAParkourMarkerType::JumpToPoint);
	ATAParkourMarker* JumpNear = F.SpawnMarker(FVector(60.f, 0.f, 0.f), ETAParkourMarkerType::JumpToPoint);
	ATAParkourMarker* Drop = F.SpawnMarker(FVector(100.f, 50.f, 0.f), ETAParkourMarkerType::DropDown);
	TestTrue(TEXT("Marker prompt type follows MarkerType"),
		JumpNear->GetLandingPrompt(false)->PromptType == ETAPromptType::ParkourJump && Drop->GetLandingPrompt(true)->PromptType == ETAPromptType::ParkourDrop);
	TestEqual(TEXT("Drop marker default text"), Drop->GetLandingPrompt(true)->ResolveTextId(), GetDefault<UTAPromptSettings>()->ParkourDropTextId);

	F.Prompt->UpdatePrompts();
	TestTrue(TEXT("Jump prompt on the nearest eligible jump marker"), F.Prompt->GetShownSource(ETAPromptType::ParkourJump) == JumpNear);
	TestNull(TEXT("Only the shared winning marker is shown"), F.Prompt->GetShownSource(ETAPromptType::ParkourDrop));
	TestFalse(TEXT("Farther jump marker not prompted"), JumpFar->GetLandingPrompt(false)->IsPromptShown());

	F.Pawn->GetParkourComponent()->UnregisterMarker(JumpNear);
	F.Prompt->UpdatePrompts();
	TestTrue(TEXT("Leaving a marker moves both execution and prompt to the nearest remaining marker"),
		F.Prompt->GetShownSource(ETAPromptType::ParkourDrop) == Drop && F.Pawn->GetParkourComponent()->SelectParkourMarker() == Drop);
	TestFalse(TEXT("Left marker's prompt hidden"), JumpNear->GetLandingPrompt(false)->IsPromptShown());
	return true;
}

#endif
