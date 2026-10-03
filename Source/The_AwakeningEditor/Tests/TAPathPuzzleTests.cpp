#include "Puzzle/TAPathPuzzleSession.h"
#include "Puzzle/TAPathPuzzleRules.h"
#include "Puzzle/TAPuzzleRewards.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAPlayerState.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"

namespace
{
	FTAPuzzleDefinition Fixture()
	{
		FTAPuzzleDefinition D;
		D.Nodes.SetNum(4); D.StartNode = 0; D.EndNode = 3;
		D.MaxEnergy = 40; D.MaxUnits = 2; D.TimeLimit = 20.f;
		for (int32 I = 0; I < 3; ++I)
		{
			FTAPuzzleEdge E; E.From = I; E.To = I+1; E.Energy = 10; D.Edges.Add(E);
		}
		D.Nodes[1].Effect.Type = ETAPuzzleEffectType::Money; D.Nodes[1].Effect.Amount = 10;
		D.Nodes[2].Effect.Type = ETAPuzzleEffectType::EnergyLimit; D.Nodes[2].Effect.Amount = 5;
		return D;
	}
	UTAPathPuzzleSession* NewSession(const FTAPuzzleDefinition& D, const FTAPuzzleSettings& Settings)
	{
		UTAPathPuzzleSession* S = NewObject<UTAPathPuzzleSession>();
		FString Error; check(S->Initialize(D, Settings, Error)); return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPuzzleSessionTest, "TheAwakening.Puzzle.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPuzzleSessionTest::RunTest(const FString& Parameters)
{
	FTAPuzzleSettings Settings;
	TestEqual(TEXT("Default undo budget is zero"), Settings.UndoAllowance, 0);
	TestEqual(TEXT("Default retry budget is zero"), Settings.RetryAllowance, 0);
	Settings.bEnableUndo = true; Settings.UndoAllowance = 1;
	Settings.bEnableRetry = true; Settings.RetryAllowance = 1;
	UTAPathPuzzleSession* S = NewSession(Fixture(), Settings);
	S->AdvanceTime(3.f);
	TestEqual(TEXT("No countdown before first Start"), S->TimeRemaining, 20.f);
	TestFalse(TEXT("Must start at Start"), S->SelectNode(1));
	TestTrue(TEXT("Start"), S->SelectNode(0));
	TestFalse(TEXT("Nonadjacent node rejected"), S->SelectNode(2));
	S->SelectNode(1); S->AdvanceTime(3.f);
	TestEqual(TEXT("Pending reward collected"), S->Progress.CollectedEffects.Num(), 1);
	TestTrue(TEXT("Undo available"), S->Undo());
	TestEqual(TEXT("Undo removes pending reward"), S->Progress.CollectedEffects.Num(), 0);
	TestEqual(TEXT("Undo does not refund time"), S->TimeRemaining, 17.f);
	S->SelectNode(1); S->SelectNode(2);
	TestFalse(TEXT("Used-up undo cannot be repeated"), S->Undo());
	TestEqual(TEXT("Limit effect applied"), S->Progress.MaxEnergy, 45);
	TestTrue(TEXT("Retry allowed before settlement"), S->Retry());
	TestEqual(TEXT("Retry resets changed limit"), S->Progress.MaxEnergy, 40);
	TestEqual(TEXT("Retry clears pending rewards"), S->Progress.CollectedEffects.Num(), 0);
	TestEqual(TEXT("Retry does not refill undo"), S->GetUndoRemaining(), 0);
	TestEqual(TEXT("Retry costs a use"), S->GetRetryRemaining(), 0);
	TestEqual(TEXT("Retry does not reset timer"), S->TimeRemaining, 17.f);
	S->AdvanceTime(2.f);
	TestEqual(TEXT("Timer runs even before selecting Start again"), S->TimeRemaining, 15.f);
	S->SetUndoAllowance(true, 2);
	TestEqual(TEXT("Skill adds allowance without clearing used count"), S->GetUndoRemaining(), 1);
	S->SelectNode(0); S->SelectNode(1); S->SelectNode(2); S->SelectNode(3);
	TestEqual(TEXT("Success"), S->State, ETAPuzzleState::Succeeded);
	TestEqual(TEXT("Success settlement contains main-game effect once"), S->Result.MainGameEffects.Num(), 1);
	TestFalse(TEXT("Cannot replay after settlement"), S->Retry());
	TestFalse(TEXT("Cannot undo after settlement"), S->Undo());
	TestFalse(TEXT("Cannot repeat final node"), S->SelectNode(3));
	S->AdvanceTime(100.f);
	TestEqual(TEXT("Terminal result unchanged"), S->Result.MainGameEffects.Num(), 1);
	FString Error;
	TestFalse(TEXT("Same session cannot be reinitialized to refill budgets"), S->Initialize(Fixture(), Settings, Error));
	TestEqual(TEXT("New entry refills configured allowance"), NewSession(Fixture(), Settings)->GetUndoRemaining(), 1);

	S = NewSession(Fixture(), Settings);
	S->SelectNode(0); S->SelectNode(1); S->AdvanceTime(20.f);
	TestEqual(TEXT("Timeout fails"), S->Result.Failure, ETAPuzzleFailure::TimeExpired);
	TestEqual(TEXT("Failure also settles previously collected rewards"), S->Result.MainGameEffects.Num(), 1);
	TestFalse(TEXT("Failure is terminal even with retry budget"), S->Retry());

	Settings.bEnableMainGameEffects = false;
	S = NewSession(Fixture(), Settings); S->SelectNode(0); S->SelectNode(1); S->SelectNode(2); S->SelectNode(3);
	TestEqual(TEXT("Main rewards independently disabled"), S->Result.MainGameEffects.Num(), 0);
	TestEqual(TEXT("Minigame effects still active"), S->Progress.MaxEnergy, 45);
	Settings.bEnableMainGameEffects = true; Settings.bEnableMinigameEffects = false;
	S = NewSession(Fixture(), Settings); S->SelectNode(0); S->SelectNode(1); S->SelectNode(2); S->SelectNode(3);
	TestEqual(TEXT("Minigame effects independently disabled"), S->Progress.MaxEnergy, 40);
	TestEqual(TEXT("Main rewards still collected"), S->Result.MainGameEffects.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPuzzleRealtimeTimerTest, "TheAwakening.Puzzle.RealtimeTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPuzzleRealtimeTimerTest::RunTest(const FString& Parameters)
{
	FTAPuzzleSettings Settings; Settings.bEnableRetry = true; Settings.RetryAllowance = 1;
	auto* S = NewSession(Fixture(), Settings);
	S->UpdateTimerAt(1000.0);
	S->UpdateTimerAt(1100.0);
	TestEqual(TEXT("Ready time is not charged"), S->TimeRemaining, 20.f);
	S->SelectNode(0);
	const double Start = S->LastTimerUpdateSeconds;
	// Four update opportunities separated by 1/3 real second. Slate's .125 clamp
	// is deliberately absent from the timer interface.
	for (int32 I = 1; I <= 4; ++I) S->UpdateTimerAt(Start + I / 3.0);
	TestTrue(TEXT("Background consumes real elapsed, not four clamped deltas"),
		FMath::IsNearlyEqual(S->TimeRemaining, 20.f - 4.f / 3.f, .0001f));
	const float Remaining = S->TimeRemaining;
	S->UpdateTimerAt(Start + 4.0 / 3.0);
	S->UpdateTimerAt(Start + 1.0);
	TestEqual(TEXT("Duplicate or stale samples cannot charge time twice"), S->TimeRemaining, Remaining);
	S->Retry(); S->SelectNode(0);
	TestEqual(TEXT("Retry/reselect do not reset consumed clock position"), S->LastTimerUpdateSeconds, Start + 4.0 / 3.0);
	S->UpdateTimerAt(Start + 2.0);
	TestTrue(TEXT("Retry retains continuous countdown"), FMath::IsNearlyEqual(S->TimeRemaining, 18.f, .0001f));
	S->UpdateTimerAt(Start + 30.0);
	TestEqual(TEXT("Long tick gap settles timeout"), S->Result.Failure, ETAPuzzleFailure::TimeExpired);
	S->UpdateTimerAt(Start + 60.0);
	TestEqual(TEXT("Terminal session never deducts again"), S->TimeRemaining, 0.f);
	S = NewSession(Fixture(), Settings); S->SelectNode(0);
	S->Abort(); const float AbortedRemaining = S->TimeRemaining;
	S->UpdateTimerAt(S->LastTimerUpdateSeconds + 100.0);
	TestEqual(TEXT("Aborted session is not charged"), S->TimeRemaining, AbortedRemaining);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPuzzleLimitsTest, "TheAwakening.Puzzle.Limits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPuzzleLimitsTest::RunTest(const FString& Parameters)
{
	FTAPuzzleSettings Settings;
	FTAPuzzleDefinition D = Fixture(); D.MaxEnergy = 5;
	D.Nodes[1].Effect.Type = ETAPuzzleEffectType::EnergyLimit; D.Nodes[1].Effect.Amount = 100;
	auto* S = NewSession(D, Settings); S->SelectNode(0); S->SelectNode(1);
	TestEqual(TEXT("Travel paid before destination reward"), S->Result.Failure, ETAPuzzleFailure::EnergyLimit);
	TestTrue(TEXT("Unaffordable node effects not collected"), S->Result.MainGameEffects.IsEmpty());
	D = Fixture(); D.Nodes[1].Effect.Type = ETAPuzzleEffectType::UnitLimit; D.Nodes[1].Effect.Amount = -1;
	S = NewSession(D, Settings); S->SelectNode(0); S->SelectNode(1); S->SelectNode(2);
	TestEqual(TEXT("Unit limit enforced"), S->Result.Failure, ETAPuzzleFailure::UnitLimit);
	D = Fixture(); D.Nodes[2].Effect.Amount = -30;
	S = NewSession(D, Settings); S->SelectNode(0); S->SelectNode(1); S->SelectNode(2);
	TestEqual(TEXT("Penalty lowering limit fails immediately"), S->Result.Failure, ETAPuzzleFailure::EnergyLimit);
	TestFalse(TEXT("Generator uses same penalty rules"), TAPathPuzzleRules::IsSolution(D, Settings, {0, 1, 2, 3}));
	D = Fixture(); D.Edges[1].From = 2; D.Edges[1].To = 1;
	S = NewSession(D, Settings); S->SelectNode(0); S->SelectNode(1);
	TestFalse(TEXT("No repeat nodes"), S->SelectNode(0));
	TestTrue(TEXT("Reverse edge travel supported"), S->SelectNode(2));
	const FTAPuzzleEdge DuplicateEdge = D.Edges[0];
	D.Edges.Add(DuplicateEdge); FString Error;
	TestFalse(TEXT("Duplicate edges rejected"), TAPathPuzzleRules::Validate(D, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPuzzleGeneratorTest, "TheAwakening.Puzzle.Generator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPuzzleGeneratorTest::RunTest(const FString& Parameters)
{
	for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
	for (int32 Mode = 0; Mode < 4; ++Mode)
	for (int32 Seed = 0; Seed < 8; ++Seed)
	{
		FTAPuzzleSettings Settings;
		Settings.Difficulty = static_cast<ETAPuzzleDifficulty>(Difficulty);
		Settings.bEnableMinigameEffects = (Mode & 1) != 0;
		Settings.bEnableMainGameEffects = (Mode & 2) != 0;
		const auto D = TAPathPuzzleRules::Generate(Settings, Seed);
		FString Error; TestTrue(TEXT("Generated definition valid"), TAPathPuzzleRules::Validate(D, Error));
		int32 Solutions = 0; bool bBackward = false;
		TArray<int32> Path{D.StartNode};
		TFunction<void(int32)> Visit = [&](int32 Node)
		{
			if (Solutions >= 3 && bBackward) return;
			if (Node == D.EndNode)
			{
				// Replay through the real session, independently of generator's solution classifier.
				auto* S = NewSession(D, Settings);
				for (int32 Index : Path) S->SelectNode(Index);
				if (S->State == ETAPuzzleState::Succeeded)
				{
					++Solutions;
					for (int32 I = 1; I < Path.Num(); ++I) bBackward |= D.Nodes[Path[I]].Column < D.Nodes[Path[I-1]].Column;
				}
				return;
			}
			for (const auto& E : D.Edges)
			{
				const int32 Next = E.From == Node ? E.To : (E.To == Node ? E.From : INDEX_NONE);
				if (Next != INDEX_NONE && !Path.Contains(Next)) { Path.Add(Next); Visit(Next); Path.Pop(); }
			}
		};
		Visit(D.StartNode);
		TestTrue(TEXT("At least three real playable solutions"), Solutions >= 3);
		TestTrue(TEXT("Includes backward solution"), bBackward);
		TestTrue(TEXT("Generated countdown in bounds"), D.TimeLimit >= 15.f && D.TimeLimit <= 30.f);
		for (const auto& Node : D.Nodes)
		{
			if (!Settings.bEnableMainGameEffects) TestFalse(TEXT("No disabled main effects generated"), Node.Effect.IsMainGame());
			if (!Settings.bEnableMinigameEffects) TestTrue(TEXT("No disabled mini effects generated"), Node.Effect.Type == ETAPuzzleEffectType::None || Node.Effect.IsMainGame());
		}
	}
	FTAPuzzleSettings Settings; Settings.TimeLimitOverride = 42.f;
	const auto A = TAPathPuzzleRules::Generate(Settings, 123);
	const auto B = TAPathPuzzleRules::Generate(Settings, 123);
	TestEqual(TEXT("Seed reproducible: nodes"), A.Nodes.Num(), B.Nodes.Num());
	TestEqual(TEXT("Seed reproducible: edges"), A.Edges.Num(), B.Edges.Num());
	TestEqual(TEXT("Seed reproducible: energy"), A.MaxEnergy, B.MaxEnergy);
	TestEqual(TEXT("Configured timer override"), A.TimeLimit, 42.f);
	for (int32 I = 0; I < A.Edges.Num(); ++I)
	{
		TestEqual(TEXT("Same edge from"), A.Edges[I].From, B.Edges[I].From);
		TestEqual(TEXT("Same edge cost"), A.Edges[I].Energy, B.Edges[I].Energy);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPuzzleRewardsTest, "TheAwakening.Puzzle.Rewards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPuzzleRewardsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* PC = World->SpawnActor<APlayerController>();
	ATAPlayerState* PS = World->SpawnActor<ATAPlayerState>();
	PC->SetPlayerState(PS);
	FTAPuzzleEffect E; E.Type = ETAPuzzleEffectType::Money; E.Amount = 10;
	TestEqual(TEXT("Money reward applied"), TAPuzzleRewards::ApplyDefault(PC, E), 10);
	E.Amount = -30;
	TestEqual(TEXT("Money penalty clamps and reports actual amount"), TAPuzzleRewards::ApplyDefault(PC, E), -10);
	TestEqual(TEXT("Money stays nonnegative"), PS->GetMoney(), 0);
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	// This isolated world never runs normal component initialization / BeginPlay.
	ASC->AddAttributeSetSubobject(PS->GetAttributeSet());
	ASC->InitAbilityActorInfo(PS, PS);
	ASC->SetNumericAttributeBase(UTAAttributeSet::GetMaxHealthAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), 100.f);
	E.Type = ETAPuzzleEffectType::MaxHealth; E.Amount = 5;
	TestEqual(TEXT("Max health reward applied"), TAPuzzleRewards::ApplyDefault(PC, E), 5);
	E.Type = ETAPuzzleEffectType::Health; E.Amount = -200;
	TestEqual(TEXT("Health penalty clamps at zero"), TAPuzzleRewards::ApplyDefault(PC, E), -100);
	E.Type = ETAPuzzleEffectType::Item; E.Amount = 1;
	TestEqual(TEXT("Missing item/pawn explicitly returns no grant"), TAPuzzleRewards::ApplyDefault(PC, E), 0);
	World->DestroyWorld(false);
	return true;
}
#endif
