#include "Puzzle/TAPathPuzzleRules.h"

namespace
{
	void ApplyEffect(const FTAPuzzleEffect& Effect, const FTAPuzzleSettings& Settings, FTAPuzzleProgress& P)
	{
		if (Effect.Type == ETAPuzzleEffectType::None ||
			(Effect.IsMainGame() ? !Settings.bEnableMainGameEffects : !Settings.bEnableMinigameEffects)) return;
		switch (Effect.Type)
		{
		case ETAPuzzleEffectType::EnergyLimit: P.MaxEnergy = FMath::Max(1, P.MaxEnergy + Effect.Amount); break;
		case ETAPuzzleEffectType::UnitLimit: P.MaxUnits = FMath::Max(1, P.MaxUnits + Effect.Amount); break;
		case ETAPuzzleEffectType::EnergyUsed: P.EnergyUsed = FMath::Max(0, P.EnergyUsed + Effect.Amount); break;
		default: break; // Main-game effects are collected, never applied during movement.
		}
		P.CollectedEffects.Add(Effect);
	}

	ETAPuzzleFailure CheckLimits(const FTAPuzzleProgress& P)
	{
		if (P.EnergyUsed > P.MaxEnergy) return ETAPuzzleFailure::EnergyLimit;
		if (P.UnitsUsed > P.MaxUnits) return ETAPuzzleFailure::UnitLimit;
		return ETAPuzzleFailure::None;
	}

	TArray<TArray<int32>> EnumeratePaths(const FTAPuzzleDefinition& D)
	{
		TArray<TArray<int32>> Adj;
		Adj.SetNum(D.Nodes.Num());
		for (const auto& E : D.Edges) { Adj[E.From].Add(E.To); Adj[E.To].Add(E.From); }
		TArray<TArray<int32>> Paths;
		TArray<int32> Path{D.StartNode};
		int32 Visits = 0;
		TFunction<void(int32)> Visit = [&](int32 Node)
		{
			if (++Visits > 100000 || Paths.Num() >= 5000) return;
			if (Node == D.EndNode) { Paths.Add(Path); return; }
			for (int32 Next : Adj[Node])
			{
				if (!Path.Contains(Next)) { Path.Add(Next); Visit(Next); Path.Pop(); }
			}
		};
		Visit(D.StartNode);
		return Paths;
	}

	FTAPuzzleEffect RandomEffect(FRandomStream& R, const FTAPuzzleSettings& S, bool bNode)
	{
		TArray<FTAPuzzleEffect> Pool;
		auto Add = [&](ETAPuzzleEffectType Type, int32 Amount)
		{
			FTAPuzzleEffect E; E.Type = Type; E.Amount = Amount; Pool.Add(E);
		};
		const bool bPenalty = S.Difficulty != ETAPuzzleDifficulty::Easy;
		if (S.bEnableMinigameEffects)
		{
			Add(ETAPuzzleEffectType::EnergyLimit, 15);
			Add(ETAPuzzleEffectType::UnitLimit, 1);
			Add(ETAPuzzleEffectType::EnergyUsed, -10);
			if (bPenalty) { Add(ETAPuzzleEffectType::EnergyLimit, -10); Add(ETAPuzzleEffectType::UnitLimit, -1); }
		}
		if (bNode && S.bEnableMainGameEffects)
		{
			if (S.MainGameEffectPool.IsEmpty())
			{
				Add(ETAPuzzleEffectType::Money, 10); Add(ETAPuzzleEffectType::MaxHealth, 5);
				if (bPenalty) { Add(ETAPuzzleEffectType::Money, -5); Add(ETAPuzzleEffectType::Health, -5); }
			}
			else for (const auto& E : S.MainGameEffectPool)
			{
				if (E.IsMainGame() && (bPenalty || !E.IsPenalty())) Pool.Add(E);
			}
		}
		return Pool.IsEmpty() ? FTAPuzzleEffect() : Pool[R.RandRange(0, Pool.Num() - 1)];
	}

	void AddEdge(FTAPuzzleDefinition& D, int32 A, int32 B, int32 Energy)
	{
		if (TAPathPuzzleRules::FindEdge(D, A, B) != INDEX_NONE) return;
		FTAPuzzleEdge E; E.From = A; E.To = B; E.Energy = Energy; D.Edges.Add(E);
	}

	FTAPuzzleDefinition MakeGraph(FRandomStream& R, const FTAPuzzleSettings& S, bool bFallback)
	{
		FTAPuzzleDefinition D;
		const int32 Columns = bFallback ? 3 : R.RandRange(3, 4);
		TArray<TArray<int32>> Groups;
		Groups.SetNum(Columns + 2);
		for (int32 Col = 0; Col < Groups.Num(); ++Col)
		{
			const bool bEndpoint = Col == 0 || Col == Columns + 1;
			const int32 Count = bEndpoint ? 1 : (bFallback ? 3 : R.RandRange(2, 4));
			for (int32 Row = 0; Row < Count; ++Row)
			{
				FTAPuzzleNode N;
				N.Column = Col;
				N.Position = FVector2D(static_cast<float>(Col) / (Columns + 1), static_cast<float>(Row + 1) / (Count + 1));
				N.Label = FText::FromString(Col == 0 ? TEXT("Start") : (Col == Columns + 1 ? TEXT("End") : FString::Printf(TEXT("%d%c"), Col, TEXT('A') + Row)));
				if (!bEndpoint && !bFallback && R.FRand() < .2f + .1f * static_cast<int32>(S.Difficulty)) N.Effect = RandomEffect(R, S, true);
				Groups[Col].Add(D.Nodes.Add(N));
			}
		}
		D.EndNode = D.Nodes.Num() - 1;
		for (int32 Col = 0; Col < Groups.Num() - 1; ++Col)
		{
			const auto& L = Groups[Col]; const auto& Right = Groups[Col + 1];
			for (int32 I = 0; I < L.Num(); ++I)
			{
				const int32 First = I * Right.Num() / L.Num();
				const int32 Last = FMath::Min(Right.Num() - 1, FMath::DivideAndRoundUp((I + 1) * Right.Num(), L.Num()) - 1);
				for (int32 J = First; J <= Last; ++J) AddEdge(D, L[I], Right[J], bFallback ? 10 : (R.FRand() < .3f ? R.RandRange(60, 95) : R.RandRange(10, 35)));
				if (!bFallback && L.Num() > 1 && Right.Num() > 1 && R.FRand() < .65f)
					AddEdge(D, L[I], Right[FMath::Clamp(First + (R.RandRange(0, 1) ? 1 : -1), 0, Right.Num() - 1)], R.RandRange(10, 35));
			}
		}
		if (bFallback)
		{
			AddEdge(D, Groups[1][0], Groups[2][1], 10);
			AddEdge(D, Groups[1][1], Groups[2][2], 10);
			if (S.bEnableMinigameEffects)
			{
				D.Nodes[Groups[1][1]].Effect.Type = ETAPuzzleEffectType::EnergyLimit;
				D.Nodes[Groups[1][1]].Effect.Amount = 15;
			}
			else if (S.bEnableMainGameEffects) D.Nodes[Groups[1][1]].Effect = RandomEffect(R, S, true);
			D.MaxEnergy = 100; D.MaxUnits = 5; D.bUsedFallback = true;
		}
		else for (auto& E : D.Edges)
		{
			if (R.FRand() < .12f + .04f * static_cast<int32>(S.Difficulty)) E.Effect = RandomEffect(R, S, false);
		}
		return D;
	}
}

bool TAPathPuzzleRules::Validate(const FTAPuzzleDefinition& D, FString& Error)
{
	if (D.Nodes.Num() < 2 || D.Nodes.Num() > 64 || !D.Nodes.IsValidIndex(D.StartNode) ||
		!D.Nodes.IsValidIndex(D.EndNode) || D.StartNode == D.EndNode || D.MaxEnergy < 1 || D.MaxUnits < 0 ||
		!FMath::IsFinite(D.TimeLimit) || D.TimeLimit <= 0.f)
	{ Error = TEXT("Invalid puzzle nodes, endpoints or limits."); return false; }
	TSet<uint64> Keys;
	for (const auto& E : D.Edges)
	{
		if (!D.Nodes.IsValidIndex(E.From) || !D.Nodes.IsValidIndex(E.To) || E.From == E.To || E.Energy < 0)
		{ Error = TEXT("Invalid puzzle edge."); return false; }
		const uint64 Key = (static_cast<uint64>(FMath::Min(E.From, E.To)) << 32) | static_cast<uint32>(FMath::Max(E.From, E.To));
		if (Keys.Contains(Key)) { Error = TEXT("Duplicate puzzle edge."); return false; }
		Keys.Add(Key);
	}
	Error.Reset(); return true;
}

int32 TAPathPuzzleRules::FindEdge(const FTAPuzzleDefinition& D, int32 A, int32 B)
{
	return D.Edges.IndexOfByPredicate([=](const FTAPuzzleEdge& E) { return (E.From == A && E.To == B) || (E.From == B && E.To == A); });
}

ETAPuzzleFailure TAPathPuzzleRules::ApplyMove(const FTAPuzzleDefinition& D, const FTAPuzzleSettings& S, int32 Node, FTAPuzzleProgress& P)
{
	const int32 EdgeIndex = FindEdge(D, P.Path.Last(), Node);
	check(D.Nodes.IsValidIndex(Node) && EdgeIndex != INDEX_NONE);
	const FTAPuzzleEdge& E = D.Edges[EdgeIndex];
	P.Path.Add(Node); P.EnergyUsed += E.Energy;
	if (Node != D.EndNode && Node != D.StartNode) ++P.UnitsUsed;
	// Pay travel costs first: a reward at an unaffordable destination cannot rescue the move.
	if (const auto Failure = CheckLimits(P); Failure != ETAPuzzleFailure::None) return Failure;
	ApplyEffect(E.Effect, S, P);
	ApplyEffect(D.Nodes[Node].Effect, S, P);
	// Unlike the prototype, penalties that lower a limit below usage fail immediately.
	return CheckLimits(P);
}

bool TAPathPuzzleRules::IsSolution(const FTAPuzzleDefinition& D, const FTAPuzzleSettings& S, const TArray<int32>& Path)
{
	if (Path.Num() < 2 || Path[0] != D.StartNode || Path.Last() != D.EndNode) return false;
	FTAPuzzleProgress P; P.MaxEnergy = D.MaxEnergy; P.MaxUnits = D.MaxUnits; P.Path.Add(D.StartNode);
	for (int32 I = 1; I < Path.Num(); ++I)
	{
		if (!D.Nodes.IsValidIndex(Path[I]) || P.Path.Contains(Path[I]) || P.Path.Last() == D.EndNode || FindEdge(D, P.Path.Last(), Path[I]) == INDEX_NONE) return false;
		if (ApplyMove(D, S, Path[I], P) != ETAPuzzleFailure::None) return false;
	}
	return true;
}

FTAPuzzleDefinition TAPathPuzzleRules::Generate(const FTAPuzzleSettings& S, int32 Seed)
{
	FRandomStream R(Seed);
	for (int32 Attempt = 0; Attempt < 40; ++Attempt)
	{
		FTAPuzzleDefinition D = MakeGraph(R, S, false);
		const auto Paths = EnumeratePaths(D);
		if (Paths.Num() < 5) continue;
		D.MaxUnits = D.Nodes[D.EndNode].Column - 1 + 2;
		TArray<int32> Energies;
		for (const auto& Path : Paths)
		{
			if (Path.Num() - 2 > D.MaxUnits) continue;
			int32 Energy = 0;
			for (int32 I = 1; I < Path.Num(); ++I) Energy += D.Edges[FindEdge(D, Path[I-1], Path[I])].Energy;
			Energies.AddUnique(Energy);
		}
		Energies.Sort();
		for (int32 Candidate = 0; Candidate < FMath::Min(12, Energies.Num()); ++Candidate)
		{
			D.MaxEnergy = Energies[Candidate];
			int32 Valid = 0; bool bBackward = false;
			for (const auto& Path : Paths) if (IsSolution(D, S, Path))
			{
				++Valid;
				for (int32 I = 1; I < Path.Num(); ++I) bBackward |= D.Nodes[Path[I]].Column < D.Nodes[Path[I-1]].Column;
			}
			if (Valid >= 3 && Valid <= FMath::Max(5, Paths.Num() * 2 / 5) && bBackward)
			{
				D.Seed = Seed;
				D.TimeLimit = S.TimeLimitOverride > 0.f ? S.TimeLimitOverride : FMath::Clamp(15.f + (D.Nodes.Num() - 8) * 2.f + Paths.Num() / 20, 15.f, 30.f);
				return D;
			}
		}
	}
	// Verified topology: three affordable lanes plus a backward route; never return an unsolved random fallback.
	FTAPuzzleDefinition D = MakeGraph(R, S, true);
	D.Seed = Seed;
	D.TimeLimit = S.TimeLimitOverride > 0.f ? S.TimeLimitOverride : 25.f;
	return D;
}
