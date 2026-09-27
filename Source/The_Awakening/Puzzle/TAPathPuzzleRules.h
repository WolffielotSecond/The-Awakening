#pragma once
#include "Puzzle/TAPathPuzzleTypes.h"

/** Shared by generation validation and actual play so effects cannot create false solutions. */
namespace TAPathPuzzleRules
{
	THE_AWAKENING_API bool Validate(const FTAPuzzleDefinition& Definition, FString& Error);
	THE_AWAKENING_API int32 FindEdge(const FTAPuzzleDefinition& Definition, int32 A, int32 B);
	THE_AWAKENING_API ETAPuzzleFailure ApplyMove(const FTAPuzzleDefinition& Definition,
		const FTAPuzzleSettings& Settings, int32 Node, FTAPuzzleProgress& Progress);
	THE_AWAKENING_API bool IsSolution(const FTAPuzzleDefinition& Definition,
		const FTAPuzzleSettings& Settings, const TArray<int32>& Path);
	THE_AWAKENING_API FTAPuzzleDefinition Generate(const FTAPuzzleSettings& Settings, int32 Seed);
}
