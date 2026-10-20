#include "Document/TACharacterActionComponent.h"
#include "Health/TAHealthComponent.h"
#include "Health/TADamageLibrary.h"
#include "Core/TAGameplayTags.h"
#include "Core/TATimeSubsystem.h"
#include "Core/TAPlayerState.h"
#include "Core/TAPlayerInput.h"
#include "Tests/TACharacterActionTestListener.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TACharacterActionTests
{
	/** 真实玩家环境（项目 PC / 角色蓝图），与复活测试相同 */
	struct FFixture
	{
		UWorld* World = nullptr;
		AThe_AwakeningPlayerController* PC = nullptr;
		AThe_AwakeningCharacter* Pawn = nullptr;
		UTAHealthComponent* Health = nullptr;
		UTACharacterActionComponent* Action = nullptr;
		UTATimeSubsystem* Time = nullptr;
		UTACharacterActionTestListener* Listener = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
			UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			Time = World->GetSubsystem<UTATimeSubsystem>();
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
			Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			Health = Pawn->GetHealthComponent();
			Action = Pawn->GetCharacterActionComponent();
			// 测试世界不跑 BeginPlay：手动让动作组件订阅死亡 / 复活
			if (Action) Action->BeginPlay();
			Listener = NewObject<UTACharacterActionTestListener>();
			if (Action) Listener->Bind(Action);
		}

		~FFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		bool Validate(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Controller BP"), PC) && Test.TestNotNull(TEXT("Character BP"), Pawn)
				&& Test.TestNotNull(TEXT("Health component"), Health) && Test.TestNotNull(TEXT("Action component"), Action)
				&& Test.TestNotNull(TEXT("Time subsystem"), Time);
		}

		void Advance(float Seconds) { Time->AdvanceGameplayTime(Seconds); }
		/** 只看路由器裁决，不受无界面运行时焦点归属影响 */
		bool Allows(ETAInputCapability Capability) const { return PC->GetInputWinner().Request.Allowed.Contains(Capability); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACharacterActionFallbackTest, "TheAwakening.Document.CharacterAction.Fallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACharacterActionFallbackTest::RunTest(const FString&)
{
	TACharacterActionTests::FFixture F;
	if (!F.Validate(*this)) return false;

	TArray<bool> Results;
	F.Action->PlayAction(TAGameplayTags::Anim_Pickup, [&Results](bool bInterrupted) { Results.Add(bInterrupted); });
	TestTrue(TEXT("Requested once"), F.Listener->Requested.Num() == 1);
	TestTrue(TEXT("No animation: fallback keyframe fires at start"), F.Listener->Keyframes == TArray<FName>({ TEXT("HideWorldMesh") }));
	TestTrue(TEXT("Active"), F.Action->IsActionActive());

	// 拿起文本：禁止跑酷和交互、无敌，移动照常
	TestFalse(TEXT("Parkour blocked"), F.Allows(ETAInputCapability::Parkour));
	TestFalse(TEXT("Interact blocked"), F.Allows(ETAInputCapability::Interact));
	TestTrue(TEXT("Move allowed"), F.Allows(ETAInputCapability::Move));
	TestTrue(TEXT("Inventory toggle allowed"), F.Allows(ETAInputCapability::InventoryToggle));
	TestTrue(TEXT("Invulnerable"), F.Health->HasProtection(ETAHealthProtection::Invulnerable));

	F.Advance(0.5f);
	TestTrue(TEXT("Still active before FallbackDuration"), F.Action->IsActionActive() && Results.IsEmpty());
	F.Advance(0.2f);
	TestFalse(TEXT("Finished after FallbackDuration"), F.Action->IsActionActive());
	TestTrue(TEXT("Callback once, not interrupted"), Results == TArray<bool>({ false }));
	TestTrue(TEXT("Finished event"), F.Listener->Finished == TArray<bool>({ false }));
	TestTrue(TEXT("Parkour restored"), F.Allows(ETAInputCapability::Parkour));
	TestTrue(TEXT("Interact restored"), F.Allows(ETAInputCapability::Interact));
	TestFalse(TEXT("Invulnerability removed"), F.Health->HasProtection(ETAHealthProtection::Invulnerable));

	// 塞入日记本：禁止移动和跑酷，不无敌
	F.Action->PlayAction(TAGameplayTags::Anim_InsertPage);
	TestFalse(TEXT("Insert page blocks move"), F.Allows(ETAInputCapability::Move));
	TestFalse(TEXT("Insert page blocks parkour"), F.Allows(ETAInputCapability::Parkour));
	TestFalse(TEXT("Insert page is not invulnerable"), F.Health->HasProtection(ETAHealthProtection::Invulnerable));
	F.Advance(1.f);
	TestTrue(TEXT("Move restored"), F.Allows(ETAInputCapability::Move));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACharacterActionAcceptedTest, "TheAwakening.Document.CharacterAction.Accepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACharacterActionAcceptedTest::RunTest(const FString&)
{
	TACharacterActionTests::FFixture F;
	if (!F.Validate(*this)) return false;
	F.Listener->bAcceptRequests = true;

	TArray<bool> Results;
	F.Action->PlayAction(TAGameplayTags::Anim_Pickup, [&Results](bool bInterrupted) { Results.Add(bInterrupted); });
	TestTrue(TEXT("Accepted by animation"), F.Action->IsActionAccepted());
	TestTrue(TEXT("No fallback keyframes when animated"), F.Listener->Keyframes.IsEmpty());
	F.Advance(2.f);
	TestTrue(TEXT("Waits for the animation past FallbackDuration"), F.Action->IsActionActive());

	F.Action->NotifyActionKeyframe(TAGameplayTags::Anim_PutDown, TEXT("Wrong"));
	F.Action->NotifyActionKeyframe(TAGameplayTags::Anim_Pickup, TEXT("HideWorldMesh"));
	TestTrue(TEXT("Only keyframes of the active action pass"), F.Listener->Keyframes == TArray<FName>({ TEXT("HideWorldMesh") }));
	F.Action->NotifyActionFinished(TAGameplayTags::Anim_PutDown);
	TestTrue(TEXT("Finish of another action ignored"), F.Action->IsActionActive());
	F.Action->NotifyActionFinished(TAGameplayTags::Anim_Pickup);
	TestTrue(TEXT("Animation finish completes"), Results == TArray<bool>({ false }));

	// 认领后一直不结束：MaxDuration 保底
	AddExpectedMessage(TEXT("NotifyActionFinished never came"), EAutomationExpectedErrorFlags::Contains, 1, false);
	F.Action->PlayAction(TAGameplayTags::Anim_Pickup);
	F.Advance(4.9f);
	TestTrue(TEXT("Still waiting before MaxDuration"), F.Action->IsActionActive());
	F.Advance(0.2f);
	TestFalse(TEXT("Safety timeout completes"), F.Action->IsActionActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACharacterActionInterruptTest, "TheAwakening.Document.CharacterAction.Interrupt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACharacterActionInterruptTest::RunTest(const FString&)
{
	TACharacterActionTests::FFixture F;
	if (!F.Validate(*this)) return false;

	TArray<bool> First, Second;
	F.Action->PlayAction(TAGameplayTags::Anim_Pickup, [&First](bool bInterrupted) { First.Add(bInterrupted); });
	F.Action->PlayAction(TAGameplayTags::Anim_PutDown, [&Second](bool bInterrupted) { Second.Add(bInterrupted); });
	TestTrue(TEXT("New action interrupts the old one"), First == TArray<bool>({ true }));
	TestEqual(TEXT("Second action active"), F.Action->GetActiveAction(), TAGameplayTags::Anim_PutDown.GetTag());
	F.Action->InterruptAction();
	F.Action->InterruptAction();
	TestTrue(TEXT("Interrupt reports once"), Second == TArray<bool>({ true }));
	TestTrue(TEXT("Restrictions released after interrupt"), F.Allows(ETAInputCapability::Parkour)
		&& !F.Health->HasProtection(ETAHealthProtection::Invulnerable));

	// 插纸动作（不无敌）期间死亡：立即打断
	TArray<bool> Third;
	F.Action->PlayAction(TAGameplayTags::Anim_InsertPage, [&Third](bool bInterrupted) { Third.Add(bInterrupted); });
	UTADamageLibrary::ApplyDamage(F.Pawn, F.Pawn, 100000.f, ETADamageType::Forced, TAGameplayTags::Damage_Source_Console);
	TestTrue(TEXT("Death interrupts the action"), Third == TArray<bool>({ true }));
	TestFalse(TEXT("No action after death"), F.Action->IsActionActive());

	AddExpectedMessage(TEXT("invalid action tag"), EAutomationExpectedErrorFlags::Contains, 1, false);
	TArray<bool> Invalid;
	F.Action->PlayAction(FGameplayTag(), [&Invalid](bool bInterrupted) { Invalid.Add(bInterrupted); });
	TestTrue(TEXT("Invalid tag reports interrupted immediately"), Invalid == TArray<bool>({ true }));
	return true;
}

#endif
