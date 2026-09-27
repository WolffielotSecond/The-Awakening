#pragma once
#include "CoreMinimal.h"
#include "Puzzle/TAPathPuzzleTypes.h"

namespace TAPuzzleRewards
{
	/** Returns the signed amount actually applied; item overflow is reported, never silently retried. */
	THE_AWAKENING_API int32 ApplyDefault(APlayerController* Player, const FTAPuzzleEffect& Effect);
}
