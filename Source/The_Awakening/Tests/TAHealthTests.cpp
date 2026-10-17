#include "AbilitySystem/TAAttributeSet.h"
#include "Health/TADamageLibrary.h"
#include "Core/TAGameplayTags.h"
#include "Health/TAHealthComponent.h"
#include "Tests/TAHealthTestListener.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Health/TARespawnSubsystem.h"
#include "Core/TAPlayerState.h"
#include "Core/TAPlayerInput.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "Scan/TAScanningComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Health/TARollbackSubsystem.h"
#include "Core/TATimeSubsystem.h"
#include "Core/TACheatManager.h"
#include "Puzzle/TAPuzzleRewards.h"
#include "Puzzle/TAPathPuzzleTypes.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TAHealthTests
{
	/** 最小测试环境：一个世界、一个挂 ASC + UTAAttributeSet 的 Actor。 */
	struct FDamageFixture
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		const UTAAttributeSet* Attributes = nullptr;

		FDamageFixture()
		{
			const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			Owner->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
			Attributes = ASC->AddSet<UTAAttributeSet>();
		}

		~FDamageFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		float Health() const { return ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute()); }
		void SetBase(const FGameplayAttribute& Attribute, float Value) { ASC->SetNumericAttributeBase(Attribute, Value); }
		bool Damage(float Amount, ETADamageType Type = ETADamageType::Normal)
		{
			return UTADamageLibrary::ApplyDamage(Owner, Owner, Amount, Type, TAGameplayTags::Damage_Source_Console);
		}
	};

	/** 真实玩家环境：项目的 PC / 角色蓝图 + 带 ASC 的 ATAPlayerState，走真实 Possess → InitAbilityActorInfo。 */
	struct FPlayerFixture
	{
		UWorld* World = nullptr;
		AThe_AwakeningPlayerController* PC = nullptr;
		AThe_AwakeningCharacter* Pawn = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UTAHealthComponent* Health = nullptr;
		UTARespawnSubsystem* Respawn = nullptr;

		FPlayerFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			// 需要初始化 Actor 组件：ASC 在 InitializeComponent 时收集 PlayerState 上的 AttributeSet
			World->InitializeActorsForPlay(FURL());
			UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
			UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (!PCClass || !PawnClass) return;
			PC = World->SpawnActor<AThe_AwakeningPlayerController>(PCClass);
			Pawn = World->SpawnActor<AThe_AwakeningCharacter>(PawnClass);
			auto* Local = NewObject<ULocalPlayer>(GEngine);
			Local->PlayerController = PC; PC->Player = Local;
			PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
			// 测试世界没有 GameMode，默认生成的是普通 APlayerState，换成项目的 ATAPlayerState
			if (PC->PlayerState) PC->PlayerState->Destroy();
			PC->PlayerState = World->SpawnActor<ATAPlayerState>();
			World->AddController(PC);
			PC->Possess(Pawn);
			// 测试关卡没有地面，避开与本测试无关的边缘检测
			Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			ASC = Pawn->GetAbilitySystemComponent();
			Health = Pawn->GetHealthComponent();
			Respawn = World->GetSubsystem<UTARespawnSubsystem>();
		}

		~FPlayerFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		bool Validate(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Controller BP"), PC) && Test.TestNotNull(TEXT("Character BP"), Pawn)
				&& Test.TestNotNull(TEXT("PlayerState ASC"), ASC) && Test.TestNotNull(TEXT("Health component"), Health)
				&& Test.TestNotNull(TEXT("Respawn subsystem"), Respawn)
				&& Test.TestTrue(TEXT("Health component bound to the PlayerState ASC"), Health->GetAbilitySystemComponent() == ASC);
		}
		bool Kill() { return UTADamageLibrary::ApplyDamage(Pawn, Pawn, 10000.f, ETADamageType::Forced, TAGameplayTags::Damage_Source_Console); }
		bool Damage(float Amount, ETADamageType Type, const FGameplayTag& Source = TAGameplayTags::Damage_Source_Enemy)
		{
			return UTADamageLibrary::ApplyDamage(Pawn, Pawn, Amount, Type, Source);
		}
		float HealthValue() const { return Health->GetHealth(); }
		void SetHealth(float Value) { ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), Value); }
	};

	FTADamageModifier MakeModifier(ETADamageModifierKind Kind, float Value, FGameplayTag RequiredTag = FGameplayTag(), float Duration = 0.f)
	{
		FTADamageModifier Modifier;
		Modifier.Kind = Kind;
		Modifier.Value = Value;
		Modifier.Duration = Duration;
		if (RequiredTag.IsValid()) Modifier.Condition.RequireTags.AddTag(RequiredTag);
		return Modifier;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthDamageFormulaTest, "TheAwakening.Health.DamageFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthDamageFormulaTest::RunTest(const FString&)
{
	using namespace TAHealthTests;
	FTADamageCalculationInput Input;
	Input.Amount = 100.f;
	Input.Type = ETADamageType::Normal;
	Input.DamageTags.AddTag(TAGameplayTags::Damage_Type_Normal);
	Input.DamageTags.AddTag(TAGameplayTags::Damage_Source_Enemy);
	Input.Defense = 5.f;
	Input.DefenseBonus = 0.5f;
	const TArray<FTADamageModifier> Modifiers = {
		MakeModifier(ETADamageModifierKind::FlatVulnerability, 10.f),
		MakeModifier(ETADamageModifierKind::PercentVulnerability, 0.2f),
		MakeModifier(ETADamageModifierKind::FlatReduction, 15.f),
		MakeModifier(ETADamageModifierKind::PercentReduction, 0.3f),
		MakeModifier(ETADamageModifierKind::PercentReduction, 0.2f, TAGameplayTags::Damage_Source_Enemy),
		// 条件不匹配：只对小游戏伤害生效，不应计入
		MakeModifier(ETADamageModifierKind::PercentReduction, 0.9f, TAGameplayTags::Damage_Source_Puzzle) };
	// D1=110 D2=132 D3=132-7.5=124.5 D4=109.5 最终=109.5×0.7×0.8=61.32
	TestEqual(TEXT("Documented example"), FTADamageCalculator::Compute(Input, Modifiers), 61.32f, 0.001f);

	TestEqual(TEXT("Percent reductions multiply, not add"), FTADamageCalculator::Compute(
		{100.f, ETADamageType::Normal, Input.DamageTags, 0.f, 0.f},
		TArray<FTADamageModifier>{MakeModifier(ETADamageModifierKind::PercentReduction, 0.3f), MakeModifier(ETADamageModifierKind::PercentReduction, 0.2f)}),
		56.f, 0.001f);
	TestEqual(TEXT("Defense above damage clamps to 0"), FTADamageCalculator::Compute(
		{10.f, ETADamageType::Normal, Input.DamageTags, 50.f, 0.f}, TArray<FTADamageModifier>()), 0.f, 0.001f);
	// 每一步单独截断：防御截到 0 之后，负的固定值减伤（等于固定加伤）仍会生效
	TestEqual(TEXT("Negative flat reduction applies after the defense clamp"), FTADamageCalculator::Compute(
		{10.f, ETADamageType::Normal, Input.DamageTags, 50.f, 0.f}, TArray<FTADamageModifier>{MakeModifier(ETADamageModifierKind::FlatReduction, -5.f)}), 5.f, 0.001f);

	FGameplayTagContainer TrueTags;
	TrueTags.AddTag(TAGameplayTags::Damage_Type_True);
	TrueTags.AddTag(TAGameplayTags::Damage_Source_Puzzle);
	TestEqual(TEXT("True damage only uses true multipliers"), FTADamageCalculator::Compute(
		{40.f, ETADamageType::TrueDamage, TrueTags, 100.f, 1.f},
		TArray<FTADamageModifier>{MakeModifier(ETADamageModifierKind::TrueDamageMultiplier, 0.5f, TAGameplayTags::Damage_Source_Puzzle),
		 MakeModifier(ETADamageModifierKind::TrueDamageMultiplier, 0.5f, TAGameplayTags::Damage_Source_Enemy),
		 MakeModifier(ETADamageModifierKind::PercentReduction, 0.9f)}), 20.f, 0.001f);
	TestEqual(TEXT("Forced damage ignores everything"), FTADamageCalculator::Compute(
		{40.f, ETADamageType::Forced, FGameplayTagContainer(), 100.f, 1.f}, Modifiers), 40.f, 0.001f);

	TestTrue(TEXT("Child source tag is valid"), FTADamageCalculator::IsValidSourceTag(TAGameplayTags::Damage_Source_Enemy));
	TestFalse(TEXT("Source root is not a valid source"), FTADamageCalculator::IsValidSourceTag(TAGameplayTags::Damage_Source));
	TestFalse(TEXT("Non-source tag is rejected"), FTADamageCalculator::IsValidSourceTag(TAGameplayTags::Damage_Type_Normal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthClampTest, "TheAwakening.Health.HealthClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthClampTest::RunTest(const FString&)
{
	TAHealthTests::FDamageFixture F;
	if (!TestNotNull(TEXT("AttributeSet is registered"), F.Attributes)) return false;

	F.Damage(500.f);
	TestEqual(TEXT("Overkill clamps Health to 0"), F.Health(), 0.f);

	F.SetBase(UTAAttributeSet::GetHealthAttribute(), 150.f);
	TestEqual(TEXT("Health base cannot exceed MaxHealth"), F.Health(), 100.f);

	F.SetBase(UTAAttributeSet::GetMaxHealthAttribute(), 60.f);
	TestEqual(TEXT("Lowering MaxHealth pulls Health down"), F.Health(), 60.f);

	F.SetBase(UTAAttributeSet::GetMaxHealthAttribute(), 0.f);
	TestEqual(TEXT("MaxHealth stays at least 1"), F.Attributes->GetMaxHealth(), 1.f);
	TestEqual(TEXT("Health follows MaxHealth floor"), F.Health(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthDamageEntryTest, "TheAwakening.Health.DamageEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthDamageEntryTest::RunTest(const FString&)
{
	TAHealthTests::FDamageFixture F;
	if (!TestNotNull(TEXT("AttributeSet is registered"), F.Attributes)) return false;

	F.SetBase(UTAAttributeSet::GetDefenseAttribute(), 4.f);
	F.SetBase(UTAAttributeSet::GetDefenseBonusAttribute(), 0.5f);
	F.Damage(20.f);
	TestEqual(TEXT("Normal damage subtracts defense × (1 + bonus); decimals kept"), F.Health(), 86.f, 0.001f);
	F.Damage(20.f, ETADamageType::TrueDamage);
	TestEqual(TEXT("True damage ignores defense"), F.Health(), 66.f, 0.001f);
	TestEqual(TEXT("IncomingDamage is reset after resolution"), F.Attributes->GetIncomingDamage(), 0.f);

	// 没有生命组件的 ASC：保护按状态 Tag 判断
	F.ASC->AddLooseGameplayTag(TAGameplayTags::State_Invulnerable);
	F.Damage(30.f);
	TestEqual(TEXT("Invulnerable tag blocks normal damage"), F.Health(), 66.f, 0.001f);
	F.Damage(30.f, ETADamageType::Forced);
	TestEqual(TEXT("Forced damage ignores invulnerability"), F.Health(), 36.f, 0.001f);
	F.ASC->RemoveLooseGameplayTag(TAGameplayTags::State_Invulnerable);

	F.ASC->AddLooseGameplayTag(TAGameplayTags::State_Dead);
	F.Damage(30.f, ETADamageType::Forced);
	TestEqual(TEXT("Dead state ignores even forced damage"), F.Health(), 36.f, 0.001f);
	F.ASC->RemoveLooseGameplayTag(TAGameplayTags::State_Dead);

	AddExpectedMessage(TEXT("negative amount"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedMessage(TEXT("has no AbilitySystemComponent"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedMessage(TEXT("every damage needs a source"), EAutomationExpectedErrorFlags::Contains, 2, false);
	TestFalse(TEXT("Negative damage is rejected"), F.Damage(-10.f));
	TestFalse(TEXT("Actor without ASC is rejected"), UTADamageLibrary::ApplyDamage(nullptr, F.World->SpawnActor<AActor>(), 10.f,
		ETADamageType::Normal, TAGameplayTags::Damage_Source_Console));
	TestFalse(TEXT("Missing source is rejected"), UTADamageLibrary::ApplyDamage(F.Owner, F.Owner, 10.f, ETADamageType::Normal, FGameplayTag()));
	TestFalse(TEXT("Source root is rejected"), UTADamageLibrary::ApplyDamage(F.Owner, F.Owner, 10.f, ETADamageType::Normal,
		TAGameplayTags::Damage_Source));
	TestEqual(TEXT("Rejected damage changes nothing"), F.Health(), 36.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthProtectionTest, "TheAwakening.Health.Protection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthProtectionTest::RunTest(const FString&)
{
	TAHealthTests::FPlayerFixture F;
	if (!F.Validate(*this)) return false;
	UObject* Puzzle = NewObject<UTAHealthTestListener>();
	UObject* God = NewObject<UTAHealthTestListener>();

	// 小游戏：无敌（放行 Puzzle）+ 不会死
	FTAHealthProtectionHandle Invulnerable = F.Health->AddProtection(ETAHealthProtection::Invulnerable, Puzzle,
		FGameplayTagContainer(TAGameplayTags::Damage_Source_Puzzle));
	FTAHealthProtectionHandle Undying = F.Health->AddProtection(ETAHealthProtection::Undying, Puzzle, FGameplayTagContainer());
	TestTrue(TEXT("Protection tags mirrored on ASC"), F.ASC->HasMatchingGameplayTag(TAGameplayTags::State_Invulnerable)
		&& F.ASC->HasMatchingGameplayTag(TAGameplayTags::State_Undying));
	F.Damage(30.f, ETADamageType::Normal, TAGameplayTags::Damage_Source_Enemy);
	F.Damage(30.f, ETADamageType::TrueDamage, TAGameplayTags::Damage_Source_Console);
	TestEqual(TEXT("Invulnerable blocks normal and true damage from other sources"), F.HealthValue(), 100.f, 0.001f);
	F.Damage(30.f, ETADamageType::TrueDamage, TAGameplayTags::Damage_Source_Puzzle);
	TestEqual(TEXT("Allowed puzzle source passes"), F.HealthValue(), 70.f, 0.001f);
	F.Damage(500.f, ETADamageType::TrueDamage, TAGameplayTags::Damage_Source_Puzzle);
	TestEqual(TEXT("Undying keeps 1 Health"), F.HealthValue(), 1.f, 0.001f);
	TestFalse(TEXT("Undying prevents death"), F.Health->IsDead());

	// 另一来源的无敌不放行 Puzzle：两条都在时仍被挡住
	FTAHealthProtectionHandle GodHandle = F.Health->AddProtection(ETAHealthProtection::Invulnerable, God, FGameplayTagContainer());
	F.SetHealth(50.f);
	F.Damage(10.f, ETADamageType::TrueDamage, TAGameplayTags::Damage_Source_Puzzle);
	TestEqual(TEXT("Any non-allowing invulnerability blocks"), F.HealthValue(), 50.f, 0.001f);
	F.Health->RemoveProtection(GodHandle);
	TestFalse(TEXT("Handle invalidated after removal"), GodHandle.IsValid());
	TestTrue(TEXT("Removing one owner keeps the other's tag"), F.ASC->HasMatchingGameplayTag(TAGameplayTags::State_Invulnerable));

	// 强制伤害无视两种保护
	F.Damage(50.f, ETADamageType::Forced, TAGameplayTags::Damage_Source_Console);
	TestTrue(TEXT("Forced damage kills through invulnerable + undying"), F.Health->IsDead());
	TestTrue(TEXT("Last damage source recorded"), F.Health->GetLastDamageSource() == TAGameplayTags::Damage_Source_Console);

	F.Health->RemoveProtection(Invulnerable);
	F.Health->RemoveProtection(Undying);
	TestFalse(TEXT("All protection tags removed"), F.ASC->HasMatchingGameplayTag(TAGameplayTags::State_Invulnerable)
		|| F.ASC->HasMatchingGameplayTag(TAGameplayTags::State_Undying));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthModifierListTest, "TheAwakening.Health.ModifierList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthModifierListTest::RunTest(const FString&)
{
	using namespace TAHealthTests;
	FPlayerFixture F;
	if (!F.Validate(*this)) return false;
	UTATimeSubsystem* Time = F.World->GetSubsystem<UTATimeSubsystem>();
	UObject* Armor = NewObject<UTAHealthTestListener>();
	UObject* Buff = NewObject<UTAHealthTestListener>();
	auto* Listener = NewObject<UTAHealthTestListener>();
	F.Health->OnDamageModifierExpired.AddDynamic(Listener, &UTAHealthTestListener::HandleModifierExpired);

	FTADamageModifierHandle EnemyArmor = F.Health->AddDamageModifier(
		MakeModifier(ETADamageModifierKind::PercentReduction, 0.5f, TAGameplayTags::Damage_Source_Enemy), Armor);
	F.Health->AddDamageModifier(MakeModifier(ETADamageModifierKind::PercentReduction, 0.5f, FGameplayTag(), 2.f), Buff);
	F.Damage(40.f, ETADamageType::Normal, TAGameplayTags::Damage_Source_Enemy);
	TestEqual(TEXT("Every matching modifier counts: 40×0.5×0.5"), F.HealthValue(), 90.f, 0.001f);
	F.Damage(40.f, ETADamageType::Normal, TAGameplayTags::Damage_Source_Console);
	TestEqual(TEXT("Source-specific modifier skipped for other sources"), F.HealthValue(), 70.f, 0.001f);

	Time->AdvanceGameplayTime(1.f);
	TestEqual(TEXT("Temporary modifier still active before expiry"), F.Health->GetActiveDamageModifierCount(), 2);
	Time->AdvanceGameplayTime(1.5f);
	TestEqual(TEXT("Temporary modifier expires on gameplay time"), F.Health->GetActiveDamageModifierCount(), 1);
	TestEqual(TEXT("Expiry broadcast once"), Listener->ModifierExpiredCount, 1);

	F.Health->AddDamageModifier(MakeModifier(ETADamageModifierKind::FlatReduction, 5.f, FGameplayTag(), 30.f), Buff);
	F.Health->ClearTemporaryDamageModifiers();
	TestEqual(TEXT("Clearing temporaries keeps permanent modifiers"), F.Health->GetActiveDamageModifierCount(), 1);

	TestTrue(TEXT("Owner removes its own modifier"), F.Health->RemoveDamageModifier(EnemyArmor));
	TestFalse(TEXT("Second removal is a no-op"), F.Health->RemoveDamageModifier(EnemyArmor));
	F.Health->AddDamageModifier(MakeModifier(ETADamageModifierKind::PercentReduction, 1.f), Armor);
	Armor->MarkAsGarbage();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestEqual(TEXT("Modifier of a destroyed owner no longer applies"), F.Health->GetActiveDamageModifierCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthComponentBindingTest, "TheAwakening.Health.ComponentBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthComponentBindingTest::RunTest(const FString&)
{
	TAHealthTests::FDamageFixture F;
	auto* Health = NewObject<UTAHealthComponent>(F.Owner);
	F.Owner->AddInstanceComponent(Health);
	Health->RegisterComponent();
	auto* Listener = NewObject<UTAHealthTestListener>();
	Health->OnHealthChanged.AddDynamic(Listener, &UTAHealthTestListener::HandleHealthChanged);

	TestEqual(TEXT("Unbound component reports 0 health"), Health->GetHealth(), 0.f);
	TestEqual(TEXT("Unbound component reports 0 normalized"), Health->GetHealthNormalized(), 0.f);
	TestFalse(TEXT("Unbound component is not dead"), Health->IsDead());

	Health->InitializeWithASC(F.ASC);
	if (!TestEqual(TEXT("Binding broadcasts one initial sync"), Listener->HealthEvents.Num(), 1)) return false;
	TestEqual(TEXT("Initial sync current"), Listener->HealthEvents[0].Current, 100.f);
	TestEqual(TEXT("Initial sync delta is 0"), Listener->HealthEvents[0].Delta, 0.f);

	Health->InitializeWithASC(F.ASC);
	TestEqual(TEXT("Rebinding the same ASC does not broadcast again"), Listener->HealthEvents.Num(), 1);

	F.Damage(30.f);
	if (!TestEqual(TEXT("Damage broadcasts once"), Listener->HealthEvents.Num(), 2)) return false;
	TestEqual(TEXT("Damage current"), Listener->HealthEvents[1].Current, 70.f, 0.01f);
	TestEqual(TEXT("Damage delta is negative"), Listener->HealthEvents[1].Delta, -30.f, 0.01f);
	TestEqual(TEXT("Normalized health"), Health->GetHealthNormalized(), 0.7f, 0.001f);

	Listener->HealthEvents.Reset();
	F.SetBase(UTAAttributeSet::GetMaxHealthAttribute(), 50.f);
	TestTrue(TEXT("Lowering MaxHealth broadcasts"), Listener->HealthEvents.Num() >= 1);
	TestTrue(TEXT("Last broadcast reflects lowered Health and Max"), Listener->HealthEvents.Num() > 0 &&
		FMath::IsNearlyEqual(Listener->HealthEvents.Last().Current, 50.f) && FMath::IsNearlyEqual(Listener->HealthEvents.Last().Max, 50.f));

	F.ASC->AddLooseGameplayTag(TAGameplayTags::State_Dead);
	TestTrue(TEXT("IsDead follows State.Dead"), Health->IsDead());
	F.ASC->RemoveLooseGameplayTag(TAGameplayTags::State_Dead);

	Health->UninitializeFromASC();
	Listener->HealthEvents.Reset();
	F.Damage(10.f);
	TestEqual(TEXT("Unbound component no longer broadcasts"), Listener->HealthEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthCharacterBlueprintTest, "TheAwakening.Health.CharacterBlueprintHasComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthCharacterBlueprintTest::RunTest(const FString&)
{
	UClass* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (!TestNotNull(TEXT("Real player character Blueprint"), PawnClass)) return false;
	const auto* CDO = GetDefault<AThe_AwakeningCharacter>(PawnClass);
	TestTrue(TEXT("Player character Blueprint carries UTAHealthComponent"),
		CDO->GetHealthComponent() && CDO->GetHealthComponent()->IsA<UTAHealthComponent>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADeathInputTest, "TheAwakening.Health.DeathOnceAndInputLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADeathInputTest::RunTest(const FString&)
{
	TAHealthTests::FPlayerFixture F;
	if (!F.Validate(*this)) return false;
	auto* Listener = NewObject<UTAHealthTestListener>();
	F.Health->OnDeathStarted.AddDynamic(Listener, &UTAHealthTestListener::HandleDeathStarted);
	F.Health->OnRespawned.AddDynamic(Listener, &UTAHealthTestListener::HandleRespawned);

	// 同一帧两次致命伤害只进入一次死亡
	F.Kill();
	F.Kill();
	TestEqual(TEXT("OnDeathStarted broadcasts exactly once"), Listener->DeathStartedCount, 1);
	TestTrue(TEXT("Health component reports dead"), F.Health->IsDead());
	TestTrue(TEXT("Respawn subsystem took over the death"), F.Respawn->IsRespawning());

	const auto Winner = F.PC->GetInputWinner();
	TestEqual(TEXT("Death request wins input arbitration"), Winner.Request.Priority, UTARespawnSubsystem::DeathInputPriority);
	TestTrue(TEXT("Death request belongs to the respawn subsystem"), Winner.Request.Owner.Get() == F.Respawn);
	TestTrue(TEXT("Subsystem holds the winning handle"), Winner.Handle == F.Respawn->GetDeathInputHandle());
	TestFalse(TEXT("Move denied while dead"), F.PC->AllowsInput(ETAInputCapability::Move));
	TestFalse(TEXT("Interact denied while dead"), F.PC->AllowsInput(ETAInputCapability::Interact));
	TestFalse(TEXT("Scan denied while dead"), F.PC->AllowsInput(ETAInputCapability::Scan));
	TestFalse(TEXT("Cursor hidden while dead"), Winner.Request.Presentation.bShowCursor);
	F.PC->OpenPauseMenu();
	TestFalse(TEXT("Pause menu cannot open while dead"), F.PC->IsPauseMenuOpen());

	TestFalse(TEXT("BeginRespawn during respawn is ignored"), F.Respawn->BeginRespawn(F.PC, 0));
	TestTrue(TEXT("Ignored BeginRespawn keeps the original handle"), F.PC->GetInputWinner().Handle == Winner.Handle);

	// 死亡状态下的伤害被忽略（先把 Health 拉回 50，确认致命伤害也扣不动）
	F.ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), 50.f);
	F.Kill();
	TestEqual(TEXT("Damage ignored while dead"), F.Health->GetHealth(), 50.f);
	TestEqual(TEXT("Health rising while dead does not re-enter death"), Listener->DeathStartedCount, 1);

	F.Respawn->CompleteRespawn();
	TestFalse(TEXT("State.Dead removed after respawn"), F.Health->IsDead());
	TestTrue(TEXT("Death input request released exactly"), F.PC->GetInputWinner().IsBase());
	TestFalse(TEXT("Subsystem no longer respawning"), F.Respawn->IsRespawning());
	TestEqual(TEXT("OnRespawned broadcasts once"), Listener->RespawnedCount, 1);

	// 绕过 GE 直接改 Health（模拟小游戏惩罚的路径）也能触发死亡；复活后可以再次死亡
	F.ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), 100.f);
	F.ASC->ApplyModToAttribute(UTAAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, 0.f);
	TestEqual(TEXT("Direct Health modification to 0 triggers death"), Listener->DeathStartedCount, 2);
	TestTrue(TEXT("Second death locks input again"), F.PC->GetInputWinner().Request.Priority == UTARespawnSubsystem::DeathInputPriority);
	F.Respawn->CompleteRespawn();
	TestTrue(TEXT("Second respawn releases input"), F.PC->GetInputWinner().IsBase());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADeathCleanupTest, "TheAwakening.Health.DeathCancelsScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADeathCleanupTest::RunTest(const FString&)
{
	TAHealthTests::FPlayerFixture F;
	if (!F.Validate(*this)) return false;
	auto* Scan = F.PC->FindComponentByClass<UTAScanningComponent>();
	auto* Freeze = F.World->GetSubsystem<UTAFreezeSubsystem>();
	if (!TestNotNull(TEXT("Scan component"), Scan) || !TestNotNull(TEXT("Freeze subsystem"), Freeze)) return false;
	auto* Listener = NewObject<UTAHealthTestListener>();
	Scan->OnScanEnded.AddDynamic(Listener, &UTAHealthTestListener::HandleScanEnded);

	// 与 ExternalOwnership 测试相同的做法：不需要 PIE 视口即可驱动扫描与冻结
	auto* Participant = NewObject<UTAFreezeComponent>(F.Pawn);
	F.Pawn->AddInstanceComponent(Participant); Participant->RegisterComponent();
	Freeze->RegisterParticipant(Participant);
	// BeginPlay / TickComponent 在 UTAScanningComponent 上是 protected，经基类调用（虚函数仍分派到扫描组件）
	UActorComponent* ScanAsComponent = Scan;
	ScanAsComponent->BeginPlay();
	if (!TestTrue(TEXT("Scan starts"), Scan->StartScan())) return false;
	TestTrue(TEXT("Scan owns a freeze request"), Participant->HasFreezeRequest());

	F.Kill();
	TestFalse(TEXT("Death ends active scanning"), Scan->IsScanning());
	// 死亡先申请输入请求再关扫描：扫描在输入胜者变化时以 UIInterrupted 自行取消（与 Canceled 走同一个 CancelScan 路径），
	// 之后的 SetScanEnabled(false) 已无扫描可取消，所以只结束一次。
	TestEqual(TEXT("Scan ends exactly once"), Listener->ScanEndReasons.Num(), 1);
	TestTrue(TEXT("Scan ended by the death input request"), Listener->ScanEndReasons.Num() == 1 &&
		Listener->ScanEndReasons[0] == ETAScanEndReason::UIInterrupted);
	TestFalse(TEXT("Scan permission disabled while dead"), Scan->IsScanEnabled());
	for (int32 Step = 0; Step < 20 && Participant->HasFreezeRequest(); ++Step)
	{
		ScanAsComponent->TickComponent(0.25f, LEVELTICK_All, nullptr);
		Scan->UpdateTimeFreezeBlend();
	}
	TestFalse(TEXT("Scan released its own freeze request through fade-out"), Participant->HasFreezeRequest());
	TestEqual(TEXT("Freeze strength back to zero"), Freeze->GetFreezeStrength(), 0.f);

	F.Respawn->CompleteRespawn();
	TestTrue(TEXT("Scan permission restored after respawn"), Scan->IsScanEnabled());

	// 死亡前扫描本来就被关掉时，复活后保持关闭，不替别的来源打开
	Scan->SetScanEnabled(false);
	F.ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), 100.f);
	F.Kill();
	F.Respawn->CompleteRespawn();
	TestFalse(TEXT("Respawn restores the pre-death scan setting, not unconditional true"), Scan->IsScanEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARollbackJournalTest, "TheAwakening.Health.RollbackJournal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARollbackJournalTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UTARollbackSubsystem* Rollback = World->GetSubsystem<UTARollbackSubsystem>();
	if (!TestNotNull(TEXT("Rollback subsystem"), Rollback)) { World->DestroyWorld(false); return false; }
	UObject* Dialogue = NewObject<UTAHealthTestListener>();
	UObject* Puzzle = NewObject<UTAHealthTestListener>();

	TArray<FString> Log;
	TestFalse(TEXT("Not recording without a scope"), Rollback->IsRecording());
	Rollback->RecordUndo(TEXT("Ignored"), [&] { Log.Add(TEXT("ignored")); });

	FTARollbackScopeHandle Outer = Rollback->BeginScope(Dialogue, TEXT("Dialogue"), [&] { Log.Add(TEXT("abort dialogue")); });
	Rollback->RecordUndo(TEXT("A"), [&] { Log.Add(TEXT("undo A")); });
	FTARollbackScopeHandle Inner = Rollback->BeginScope(Puzzle, TEXT("Puzzle"), [&] { Log.Add(TEXT("abort puzzle")); });
	Rollback->RecordUndo(TEXT("B"), [&] { Log.Add(TEXT("undo B")); });
	Rollback->RecordUndo(TEXT("C"), [&]
	{
		Log.Add(TEXT("undo C"));
		// 回滚过程中产生的改动不再被记录
		Rollback->RecordUndo(TEXT("Nested"), [&] { Log.Add(TEXT("nested")); });
	});
	TestEqual(TEXT("Records go to the innermost scope"), Rollback->GetRecordCount(), 3);

	Rollback->RollbackAll();
	TestTrue(TEXT("Innermost first, each scope in reverse order, then its abort action"), Log == TArray<FString>({
		TEXT("undo C"), TEXT("undo B"), TEXT("abort puzzle"), TEXT("undo A"), TEXT("abort dialogue") }));
	TestEqual(TEXT("All scopes closed"), Rollback->GetScopeCount(), 0);
	Rollback->CommitScope(Inner);
	TestFalse(TEXT("Commit after rollback is harmless and invalidates the handle"), Inner.IsValid());

	Log.Reset();
	FTARollbackScopeHandle Committed = Rollback->BeginScope(Dialogue, TEXT("Dialogue"), [&] { Log.Add(TEXT("abort")); });
	Rollback->RecordUndo(TEXT("A"), [&] { Log.Add(TEXT("undo A")); });
	Rollback->CommitScope(Committed);
	Rollback->RollbackAll();
	TestTrue(TEXT("Committed scope discards its records"), Log.IsEmpty());

	Rollback->BeginScope(Puzzle, TEXT("Puzzle"));
	Puzzle->MarkAsGarbage();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Scope of a destroyed owner counts as committed"), Rollback->IsRecording());
	(void)Outer;
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARollbackOnDeathTest, "TheAwakening.Health.RollbackOnDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARollbackOnDeathTest::RunTest(const FString&)
{
	TAHealthTests::FPlayerFixture F;
	if (!F.Validate(*this)) return false;
	UTARollbackSubsystem* Rollback = F.World->GetSubsystem<UTARollbackSubsystem>();
	ATAPlayerState* PS = F.PC->GetPlayerState<ATAPlayerState>();
	UObject* PuzzleOwner = NewObject<UTAHealthTestListener>();

	bool bAborted = false;
	Rollback->BeginScope(PuzzleOwner, TEXT("Puzzle"), [&] { bAborted = true; });
	// 小游戏结算：加钱、提高上限、回血、扣血（真实伤害），全部应被记录
	FTAPuzzleEffect Effect;
	Effect.Type = ETAPuzzleEffectType::Money; Effect.Amount = 25;
	TAPuzzleRewards::ApplyDefault(F.PC, Effect);
	Effect.Type = ETAPuzzleEffectType::MaxHealth; Effect.Amount = 20;
	TAPuzzleRewards::ApplyDefault(F.PC, Effect);
	Effect.Type = ETAPuzzleEffectType::Health; Effect.Amount = -30;
	TAPuzzleRewards::ApplyDefault(F.PC, Effect);
	TestEqual(TEXT("Penalty is applied as damage"), F.HealthValue(), 70.f, 0.001f);
	TestEqual(TEXT("Money changed during puzzle"), PS->GetMoney(), 25);

	F.Kill();
	TestTrue(TEXT("Death runs the scope's abort action"), bAborted);
	TestEqual(TEXT("Money restored"), PS->GetMoney(), 0);
	TestEqual(TEXT("MaxHealth restored"), F.Health->GetMaxHealth(), 100.f, 0.001f);
	TestEqual(TEXT("Health restored to the pre-puzzle value"), F.HealthValue(), 100.f, 0.001f);
	TestTrue(TEXT("Still dead: rollback does not cancel death"), F.Health->IsDead());
	TestEqual(TEXT("Scope closed"), Rollback->GetScopeCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAHealthCheatHealTest, "TheAwakening.Health.CheatHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAHealthCheatHealTest::RunTest(const FString&)
{
	TAHealthTests::FPlayerFixture F;
	if (!F.Validate(*this)) return false;
	auto* Cheat = NewObject<UTACheatManager>(F.PC);

	F.SetHealth(40.f);
	Cheat->TAHeal(25.f);
	TestEqual(TEXT("TAHeal <Amount> adds health"), F.HealthValue(), 65.f, 0.001f);
	Cheat->TAHeal(1000.f);
	TestEqual(TEXT("TAHeal never exceeds MaxHealth"), F.HealthValue(), F.Health->GetMaxHealth(), 0.001f);
	F.SetHealth(10.f);
	Cheat->TAHeal();
	TestEqual(TEXT("TAHeal without amount heals to full"), F.HealthValue(), F.Health->GetMaxHealth(), 0.001f);

	F.Kill();
	AddExpectedMessage(TEXT("healing does not revive"), EAutomationExpectedErrorFlags::Contains, 1, false);
	Cheat->TAHeal();
	TestEqual(TEXT("TAHeal does nothing while dead"), F.HealthValue(), 0.f, 0.001f);
	TestTrue(TEXT("Still dead"), F.Health->IsDead());
	return true;
}

#endif
