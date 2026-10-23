#include "Health/TARespawnSubsystem.h"
#include "Health/TARespawnSettings.h"
#include "Health/TACheckpoint.h"
#include "Health/TAKillVolume.h"
#include "Health/TAHealthComponent.h"
#include "Health/TADamageLibrary.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Core/TAGameplayTags.h"
#include "Core/TATimeSubsystem.h"
#include "Core/TAPlayerState.h"
#include "Core/TAPlayerInput.h"
#include "Tests/TAHealthTestListener.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"

void ATADeathPresentationTestCharacter::PlayDeathPresentation_Implementation(UTARespawnSubsystem*, int32 RunId, FGameplayTag Source, ETADamageType Type)
{
	++DeathRequests; LastRunId = RunId; LastSource = Source; LastType = Type;
	const auto* PC = Cast<AThe_AwakeningPlayerController>(GetController());
	bWasInputLocked = PC && !PC->AllowsInput(ETAInputCapability::Move) && GetHealthComponent()->IsDead();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TARespawnTests
{
	/** 与 Health 测试相同的真实玩家环境（项目 PC / 角色蓝图 + ATAPlayerState），另外带上时间与复活子系统。 */
	struct FFixture
	{
		UWorld* World = nullptr;
		AThe_AwakeningPlayerController* PC = nullptr;
		AThe_AwakeningCharacter* Pawn = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UTAHealthComponent* Health = nullptr;
		UTARespawnSubsystem* Respawn = nullptr;
		UTATimeSubsystem* Time = nullptr;

		FFixture(UClass* OverridePawnClass = nullptr)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
			UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (OverridePawnClass) PawnClass = OverridePawnClass;
			Respawn = World->GetSubsystem<UTARespawnSubsystem>();
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
			ASC = Pawn->GetAbilitySystemComponent();
			Health = Pawn->GetHealthComponent();
		}
		~FFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		bool Validate(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Controller BP"), PC) && Test.TestNotNull(TEXT("Character BP"), Pawn)
				&& Test.TestNotNull(TEXT("Health component"), Health) && Test.TestNotNull(TEXT("Respawn subsystem"), Respawn)
				&& Test.TestNotNull(TEXT("Time subsystem"), Time)
				&& Test.TestTrue(TEXT("Health bound to PlayerState ASC"), ASC && Health->GetAbilitySystemComponent() == ASC);
		}

		/** 测试世界不跑 BeginPlay，手动登记（与 ATACheckpoint::BeginPlay 做的事一致）。 */
		ATACheckpoint* SpawnCheckpoint(FName Id, const FVector& Location, bool bLevelStart = false)
		{
			auto* Checkpoint = World->SpawnActorDeferred<ATACheckpoint>(ATACheckpoint::StaticClass(), FTransform(Location));
			Checkpoint->CheckpointId = Id;
			Checkpoint->bIsLevelStart = bLevelStart;
			Checkpoint->FinishSpawning(FTransform(Location));
			Respawn->RegisterCheckpoint(Checkpoint);
			return Checkpoint;
		}

		void Kill() { UTADamageLibrary::ApplyDamage(Pawn, Pawn, 100000.f, ETADamageType::Forced, TAGameplayTags::Damage_Source_Console); }
		void Advance(float Seconds) { Time->AdvanceGameplayTime(Seconds); }
		void RunFullRespawn()
		{
			const UTARespawnSettings* S = GetDefault<UTARespawnSettings>();
			Advance(S->DeathDelay + 0.01f);
			Advance(S->FadeOutTime + 0.01f);
			Advance(S->FadeInTime + 0.01f);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADeathPresentationTest, "TheAwakening.Respawn.PresentationHooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADeathPresentationTest::RunTest(const FString&)
{
	TARespawnTests::FFixture F(ATADeathPresentationTestCharacter::StaticClass());
	if (!F.Validate(*this)) return false;
	auto* Character = CastChecked<ATADeathPresentationTestCharacter>(F.Pawn);
	F.SpawnCheckpoint(TEXT("Presentation"), FVector(800.f, 0.f, 0.f), true);
	auto* Listener = NewObject<UTAHealthTestListener>();
	F.Respawn->OnWorldReset.AddDynamic(Listener, &UTAHealthTestListener::HandleWorldReset);
	F.Kill();
	const int32 Run = Character->LastRunId;
	TestTrue(TEXT("Death hook receives locked dead character and damage context"),Character->DeathRequests==1 && Character->bWasInputLocked
		&& Character->LastSource==TAGameplayTags::Damage_Source_Console && Character->LastType==ETADamageType::Forced);
	F.Advance(3.f);
	TestTrue(TEXT("Override waits beyond default delay"),F.Respawn->GetRespawnPhase()==ETARespawnPhase::Dying && Character->BlackoutRequests==0);
	F.Respawn->CompleteDeathPresentation(Character,Run-1);
	TestEqual(TEXT("Stale completion cannot start blackout"),Character->BlackoutRequests,0);
	F.Respawn->CompleteDeathPresentation(Character,Run);
	F.Respawn->CompleteDeathPresentation(Character,Run);
	TestEqual(TEXT("Matching death completion starts blackout exactly once"),Character->BlackoutRequests,1);
	F.Advance(3.f);
	TestTrue(TEXT("Blackout override holds before reset"),F.Respawn->GetRespawnPhase()==ETARespawnPhase::FadingOut && Listener->WorldResetCount==0 && F.Health->GetHealth()==0.f);
	F.Respawn->CompleteBlackoutPresentation(Character,Run-1);
	TestEqual(TEXT("Stale blackout cannot reset world"),Listener->WorldResetCount,0);
	F.Respawn->CompleteBlackoutPresentation(Character,Run);
	F.Respawn->CompleteBlackoutPresentation(Character,Run);
	TestEqual(TEXT("Blackout completion resets exactly once"),Listener->WorldResetCount,1);
	TestTrue(TEXT("Input remains locked during fade in"),!F.PC->AllowsInput(ETAInputCapability::Move));
	F.Advance(GetDefault<UTARespawnSettings>()->FadeInTime+.01f);
	TestTrue(TEXT("Completion restores alive state and input"),!F.Health->IsDead() && F.PC->GetInputWinner().IsBase());
	F.Kill();
	TestTrue(TEXT("Next death uses a different run"),Character->LastRunId!=Run);
	F.Respawn->CompleteDeathPresentation(Character,Run);
	TestEqual(TEXT("Previous run cannot advance next death"),Character->BlackoutRequests,1);
	// An override may explicitly choose the existing defaults for an unmatched death cause.
	F.Respawn->RunDefaultDeathPresentation(Character,Character->LastRunId);
	F.Respawn->RunDefaultDeathPresentation(Character,Character->LastRunId);
	F.Advance(GetDefault<UTARespawnSettings>()->DeathDelay+.01f);
	TestEqual(TEXT("Default fallback enters blackout once"),Character->BlackoutRequests,2);
	F.Respawn->RunDefaultBlackoutPresentation(Character,Character->LastRunId);
	F.Advance(GetDefault<UTARespawnSettings>()->FadeOutTime+.01f);
	F.Advance(GetDefault<UTARespawnSettings>()->FadeInTime+.01f);
	TestFalse(TEXT("Explicit default fallback completes respawn"),F.Respawn->IsRespawning());
	F.Kill();
	AddExpectedError(TEXT("Death presentation did not complete"),EAutomationExpectedErrorFlags::Contains,2);
	F.Advance(GetDefault<UTARespawnSettings>()->PresentationMaxWait+1.f);
	TestTrue(TEXT("Missing death completion has a safety timeout"),F.Respawn->GetRespawnPhase()==ETARespawnPhase::FadingOut);
	F.Advance(GetDefault<UTARespawnSettings>()->PresentationMaxWait+1.f);
	F.Advance(GetDefault<UTARespawnSettings>()->FadeInTime+.01f);
	TestTrue(TEXT("Missing callbacks cannot leave death input locked"),!F.Respawn->IsRespawning() && F.PC->GetInputWinner().IsBase());
	F.Kill();
	Character->Destroy();
	TestTrue(TEXT("Destroyed presentation owner releases death input"),!F.Respawn->IsRespawning() && F.PC->GetInputWinner().IsBase());
	F.Advance(GetDefault<UTARespawnSettings>()->PresentationMaxWait+1.f);
	TestFalse(TEXT("Destroyed owner leaves no presentation timer"),F.Respawn->IsRespawning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARespawnFlowTest, "TheAwakening.Respawn.Flow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARespawnFlowTest::RunTest(const FString&)
{
	TARespawnTests::FFixture F;
	if (!F.Validate(*this)) return false;
	const UTARespawnSettings* S = GetDefault<UTARespawnSettings>();
	ATACheckpoint* Checkpoint = F.SpawnCheckpoint(TEXT("Camp"), FVector(1000.f, 0.f, 0.f));
	F.Respawn->ActivateCheckpoint(Checkpoint);
	const FTransform Spawn = Checkpoint->GetSpawnTransform();

	auto* Listener = NewObject<UTAHealthTestListener>();
	F.Respawn->OnWorldReset.AddDynamic(Listener, &UTAHealthTestListener::HandleWorldReset);
	F.Health->OnRespawned.AddDynamic(Listener, &UTAHealthTestListener::HandleRespawned);
	UObject* Buff = NewObject<UTAHealthTestListener>();
	FTADamageModifier Temporary; Temporary.Kind = ETADamageModifierKind::PercentReduction; Temporary.Value = 0.5f; Temporary.Duration = 999.f;
	FTADamageModifier Permanent; Permanent.Kind = ETADamageModifierKind::FlatReduction; Permanent.Value = 0.f;
	F.Health->AddDamageModifier(Temporary, Buff);
	F.Health->AddDamageModifier(Permanent, Buff);

	F.Kill();
	TestTrue(TEXT("Death enters Dying"), F.Respawn->GetRespawnPhase() == ETARespawnPhase::Dying);
	F.Advance(S->DeathDelay * 0.5f);
	TestTrue(TEXT("Still dying before DeathDelay"), F.Respawn->GetRespawnPhase() == ETARespawnPhase::Dying);
	F.Advance(S->DeathDelay * 0.5f + 0.01f);
	TestTrue(TEXT("Fade out after DeathDelay"), F.Respawn->GetRespawnPhase() == ETARespawnPhase::FadingOut);
	TestEqual(TEXT("Not reset yet"), Listener->WorldResetCount, 0);

	F.Advance(S->FadeOutTime + 0.01f);
	TestTrue(TEXT("Fading in after reset"), F.Respawn->GetRespawnPhase() == ETARespawnPhase::FadingIn);
	TestTrue(TEXT("Teleported to checkpoint spawn point"), F.Pawn->GetActorLocation().Equals(Spawn.GetLocation(), 1.f));
	TestEqual(TEXT("Health refilled by ratio"), F.Health->GetHealth(), F.Health->GetMaxHealth() * S->RespawnHealthRatio, 0.01f);
	TestEqual(TEXT("Only temporary modifiers cleared"), F.Health->GetActiveDamageModifierCount(), 1);
	TestEqual(TEXT("OnWorldReset broadcast once"), Listener->WorldResetCount, 1);
	TestTrue(TEXT("Still dead and input-locked during fade in"), F.Health->IsDead() && !F.PC->GetInputWinner().IsBase());

	F.Advance(S->FadeInTime + 0.01f);
	TestTrue(TEXT("Respawn complete"), F.Respawn->GetRespawnPhase() == ETARespawnPhase::None);
	TestFalse(TEXT("State.Dead removed"), F.Health->IsDead());
	TestTrue(TEXT("Death input released"), F.PC->GetInputWinner().IsBase());
	TestEqual(TEXT("OnRespawned once"), Listener->RespawnedCount, 1);
	TestEqual(TEXT("OnWorldReset still once per respawn"), Listener->WorldResetCount, 1);

	// 复活无敌：普通伤害无效，强制伤害仍生效；到期后恢复正常
	const float Full = F.Health->GetHealth();
	UTADamageLibrary::ApplyDamage(F.Pawn, F.Pawn, 30.f, ETADamageType::Normal, TAGameplayTags::Damage_Source_Enemy);
	TestEqual(TEXT("Respawn invulnerability blocks normal damage"), F.Health->GetHealth(), Full, 0.01f);
	F.Advance(S->RespawnInvulnerableTime + 0.01f);
	UTADamageLibrary::ApplyDamage(F.Pawn, F.Pawn, 30.f, ETADamageType::Normal, TAGameplayTags::Damage_Source_Enemy);
	TestEqual(TEXT("Damage works again after invulnerability ends"), F.Health->GetHealth(), Full - 30.f, 0.01f);
	TestFalse(TEXT("Respawn protection tag removed"), F.ASC->HasMatchingGameplayTag(TAGameplayTags::State_Invulnerable));

	// 可以再次死亡，再走一遍完整流程
	F.Kill();
	F.RunFullRespawn();
	TestEqual(TEXT("Second respawn completes"), Listener->RespawnedCount, 2);
	TestEqual(TEXT("Second OnWorldReset"), Listener->WorldResetCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARespawnCheckpointTest, "TheAwakening.Respawn.Checkpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARespawnCheckpointTest::RunTest(const FString&)
{
	TARespawnTests::FFixture F;
	if (!F.Validate(*this)) return false;

	ATACheckpoint* Start = F.SpawnCheckpoint(TEXT("Start"), FVector(-500.f, 0.f, 0.f), true);
	TestTrue(TEXT("Level-start checkpoint becomes active automatically"), F.Respawn->GetActiveCheckpoint() == Start && Start->IsActiveCheckpoint());
	ATACheckpoint* A = F.SpawnCheckpoint(TEXT("A"), FVector(500.f, 0.f, 0.f));
	ATACheckpoint* B = F.SpawnCheckpoint(TEXT("B"), FVector(1500.f, 0.f, 0.f));
	TestTrue(TEXT("Registering does not activate a normal checkpoint"), F.Respawn->GetActiveCheckpoint() == Start);

	F.Respawn->ActivateCheckpoint(A);
	F.Respawn->ActivateCheckpoint(B);
	F.Respawn->ActivateCheckpoint(A);
	TestTrue(TEXT("Last interacted wins, including going back to an old one"), F.Respawn->GetActiveCheckpoint() == A);
	TestTrue(TEXT("Only the active checkpoint shows as active"), A->IsActiveCheckpoint() && !B->IsActiveCheckpoint() && !Start->IsActiveCheckpoint());
	TestTrue(TEXT("Find by id"), F.Respawn->FindCheckpoint(TEXT("B")) == B && !F.Respawn->FindCheckpoint(TEXT("Missing")));

	F.Kill();
	F.RunFullRespawn();
	TestTrue(TEXT("Respawned at the last interacted checkpoint"), F.Pawn->GetActorLocation().Equals(A->GetSpawnTransform().GetLocation(), 1.f));

	// 没有复活点：退回 PlayerStart 并打 Warning
	F.Respawn->UnregisterCheckpoint(A);
	TestNull(TEXT("Unregistering the active checkpoint clears it"), F.Respawn->GetActiveCheckpoint());
	AddExpectedMessage(TEXT("respawning at PlayerStart"), EAutomationExpectedErrorFlags::Contains, 1, false);
	APlayerStart* PlayerStart = F.World->SpawnActor<APlayerStart>(FVector(0.f, 3000.f, 0.f), FRotator::ZeroRotator);
	F.Kill();
	F.RunFullRespawn();
	TestTrue(TEXT("Fallback to PlayerStart"), F.Pawn->GetActorLocation().Equals(PlayerStart->GetActorLocation(), 1.f));

	// 连 PlayerStart 都没有：原地复活并打 Warning
	PlayerStart->Destroy();
	AddExpectedMessage(TEXT("respawning in place"), EAutomationExpectedErrorFlags::Contains, 1, false);
	const FVector Before = F.Pawn->GetActorLocation();
	F.Kill();
	F.RunFullRespawn();
	TestTrue(TEXT("In-place respawn"), F.Pawn->GetActorLocation().Equals(Before, 1.f));
	TestFalse(TEXT("Alive again"), F.Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARespawnKillVolumeTest, "TheAwakening.Respawn.KillVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARespawnKillVolumeTest::RunTest(const FString&)
{
	TARespawnTests::FFixture F;
	if (!F.Validate(*this)) return false;
	ATAKillVolume* Volume = F.World->SpawnActor<ATAKillVolume>();
	UBoxComponent* Box = Volume ? Volume->FindComponentByClass<UBoxComponent>() : nullptr;
	if (!TestNotNull(TEXT("Kill volume box"), Box)) return false;

	// 无敌 + 不会死都挡不住杀死体积
	UObject* God = NewObject<UTAHealthTestListener>();
	F.Health->AddProtection(ETAHealthProtection::Invulnerable, God, FGameplayTagContainer());
	F.Health->AddProtection(ETAHealthProtection::Undying, God, FGameplayTagContainer());
	// 测试世界不跑物理重叠，直接触发组件的重叠委托（与引擎的调用参数一致）
	Box->OnComponentBeginOverlap.Broadcast(Box, F.Pawn, F.Pawn->GetCapsuleComponent(), 0, false, FHitResult());
	TestTrue(TEXT("Kill volume kills through every protection"), F.Health->IsDead());
	TestTrue(TEXT("Source is Environment"), F.Health->GetLastDamageSource() == TAGameplayTags::Damage_Source_Environment);

	AActor* Crate = F.World->SpawnActor<AActor>();
	Box->OnComponentBeginOverlap.Broadcast(Box, Crate, nullptr, 0, false, FHitResult());
	TestTrue(TEXT("Actors without health are ignored (no warnings)"), IsValid(Crate));
	return true;
}

#endif
