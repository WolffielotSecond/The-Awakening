#include "Puzzle/TAPathPuzzleSession.h"
#include "Puzzle/TAPathPuzzleRules.h"
#include "HAL/PlatformTime.h"

bool UTAPathPuzzleSession::Initialize(const FTAPuzzleDefinition& D, const FTAPuzzleSettings& S, FString& Error)
{
	if (bInitialized) { Error = TEXT("Create a new session for a new puzzle entry."); return false; }
	if (!TAPathPuzzleRules::Validate(D, Error)) return false;
	bInitialized = true;
	Definition = D; Settings = S;
	Settings.UndoAllowance = FMath::Max(0, Settings.UndoAllowance);
	Settings.RetryAllowance = FMath::Max(0, Settings.RetryAllowance);
	State = ETAPuzzleState::Ready;
	TimeRemaining = Definition.TimeLimit;
	ResetProgress();
	return true;
}

void UTAPathPuzzleSession::ResetProgress()
{
	Progress = FTAPuzzleProgress();
	Progress.MaxEnergy = Definition.MaxEnergy;
	Progress.MaxUnits = Definition.MaxUnits;
	History.Reset();
	LastMessage = FText::FromString(TEXT("Select Start."));
}

bool UTAPathPuzzleSession::SelectNode(int32 Index)
{
	if (!bInitialized || IsTerminal() || !Definition.Nodes.IsValidIndex(Index)) return false;
	if (Progress.Path.IsEmpty())
	{
		if (Index != Definition.StartNode)
		{
			LastMessage = FText::FromString(TEXT("Select Start first.")); OnChanged.Broadcast(); return false;
		}
		Progress.Path.Add(Index);
		// Retry remains Playing: selecting Start again must not discard elapsed time.
		if (State != ETAPuzzleState::Playing) LastTimerUpdateSeconds = FPlatformTime::Seconds();
		State = ETAPuzzleState::Playing;
		LastMessage = FText::GetEmpty(); OnChanged.Broadcast(); return true;
	}
	if (Progress.Path.Contains(Index) || TAPathPuzzleRules::FindEdge(Definition, Progress.Path.Last(), Index) == INDEX_NONE)
	{
		LastMessage = FText::FromString(TEXT("Choose a connected, unvisited node.")); OnChanged.Broadcast(); return false;
	}
	History.Add(Progress);
	const auto Failure = TAPathPuzzleRules::ApplyMove(Definition, Settings, Index, Progress);
	LastMessage = FText::GetEmpty();
	if (Failure != ETAPuzzleFailure::None || Index == Definition.EndNode) Settle(Failure);
	else OnChanged.Broadcast();
	return Failure == ETAPuzzleFailure::None;
}

bool UTAPathPuzzleSession::IsTerminal() const
{
	return State == ETAPuzzleState::Succeeded || State == ETAPuzzleState::Failed || State == ETAPuzzleState::Aborted;
}

int32 UTAPathPuzzleSession::GetUndoRemaining() const
{
	return Settings.bEnableUndo ? FMath::Max(0, Settings.UndoAllowance - UndoUsed) : 0;
}

int32 UTAPathPuzzleSession::GetRetryRemaining() const
{
	return Settings.bEnableRetry ? FMath::Max(0, Settings.RetryAllowance - RetryUsed) : 0;
}

bool UTAPathPuzzleSession::CanUndo() const
{
	return bInitialized && State == ETAPuzzleState::Playing && !History.IsEmpty() && GetUndoRemaining() > 0;
}

bool UTAPathPuzzleSession::CanRetry() const
{
	return bInitialized && State == ETAPuzzleState::Playing && GetRetryRemaining() > 0;
}

bool UTAPathPuzzleSession::Undo()
{
	if (!CanUndo()) return false;
	++UndoUsed;
	Progress = History.Pop();
	// Time is intentionally absent from snapshots: undo never refunds time or its own cost.
	LastMessage = FText::GetEmpty(); OnChanged.Broadcast(); return true;
}

bool UTAPathPuzzleSession::Retry()
{
	if (!CanRetry()) return false;
	++RetryUsed;
	ResetProgress();
	// Keep Playing and TimeRemaining unchanged, even while waiting to select Start again.
	// Retry does not reset undo usage, retry usage, or apply discarded pending rewards.
	OnChanged.Broadcast(); return true;
}

void UTAPathPuzzleSession::UpdateTimer()
{
	UpdateTimerAt(FPlatformTime::Seconds());
}

void UTAPathPuzzleSession::UpdateTimerAt(double NowSeconds)
{
	if (!FMath::IsFinite(NowSeconds)) return;
	if (!bInitialized || State != ETAPuzzleState::Playing)
	{
		LastTimerUpdateSeconds = NowSeconds;
		return;
	}
	if (NowSeconds <= LastTimerUpdateSeconds) return;
	const double Elapsed = NowSeconds - LastTimerUpdateSeconds;
	// Consume before settlement callbacks, including reentrant update opportunities.
	LastTimerUpdateSeconds = NowSeconds;
	AdvanceTime(static_cast<float>(FMath::Min(Elapsed, static_cast<double>(TimeRemaining))));
}

void UTAPathPuzzleSession::AdvanceTime(float DeltaSeconds)
{
	if (!bInitialized || State != ETAPuzzleState::Playing || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	TimeRemaining = FMath::Max(0.f, TimeRemaining - DeltaSeconds);
	if (TimeRemaining <= 0.f) Settle(ETAPuzzleFailure::TimeExpired);
}

void UTAPathPuzzleSession::SetUndoAllowance(bool bEnabled, int32 TotalAllowance)
{
	Settings.bEnableUndo = bEnabled; Settings.UndoAllowance = FMath::Max(0, TotalAllowance); OnChanged.Broadcast();
}

void UTAPathPuzzleSession::SetRetryAllowance(bool bEnabled, int32 TotalAllowance)
{
	Settings.bEnableRetry = bEnabled; Settings.RetryAllowance = FMath::Max(0, TotalAllowance); OnChanged.Broadcast();
}

void UTAPathPuzzleSession::Abort()
{
	if (!bInitialized || IsTerminal()) return;
	State = ETAPuzzleState::Aborted; OnChanged.Broadcast();
}

void UTAPathPuzzleSession::Settle(ETAPuzzleFailure Failure)
{
	if (IsTerminal()) return;
	State = Failure == ETAPuzzleFailure::None ? ETAPuzzleState::Succeeded : ETAPuzzleState::Failed;
	Result.bSucceeded = State == ETAPuzzleState::Succeeded;
	Result.Failure = Failure; Result.Progress = Progress;
	Result.TimeRemaining = TimeRemaining; Result.Seed = Definition.Seed;
	Result.MainGameEffects.Reset();
	if (Settings.bEnableMainGameEffects)
	{
		// 设计约定：成功和失败结算都交付已收集的主游戏奖惩。
		// 若以后取消“失败也应用”，在此加 Result.bSucceeded 条件即可；不要在路径点击时发奖。
		for (const auto& E : Progress.CollectedEffects) if (E.IsMainGame()) Result.MainGameEffects.Add(E);
	}
	switch (Failure)
	{
	case ETAPuzzleFailure::EnergyLimit: LastMessage = FText::FromString(TEXT("Energy limit exceeded.")); break;
	case ETAPuzzleFailure::UnitLimit: LastMessage = FText::FromString(TEXT("Unit limit exceeded.")); break;
	case ETAPuzzleFailure::TimeExpired: LastMessage = FText::FromString(TEXT("Time expired.")); break;
	default: LastMessage = FText::FromString(TEXT("Puzzle complete.")); break;
	}
	// Terminal state is set before callbacks, preventing reentrant/double settlement.
	OnSettled.Broadcast(Result);
	OnChanged.Broadcast();
}
