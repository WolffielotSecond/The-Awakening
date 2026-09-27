#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Puzzle/TAPathPuzzleTypes.h"
#include "TAPuzzleRewardReceiver.generated.h"

UINTERFACE(BlueprintType)
class THE_AWAKENING_API UTAPuzzleRewardReceiver : public UInterface
{
	GENERATED_BODY()
};

/** Optional override for all settlement rewards, e.g. a terminal or quest blueprint. */
class THE_AWAKENING_API ITAPuzzleRewardReceiver
{
	GENERATED_BODY()
public:
	/** Called once per collected effect, on success OR failure. Return actually applied item count / signed amount. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Puzzle|Rewards")
	int32 ApplyPuzzleReward(const FTAPuzzleEffect& Effect, APlayerController* Player, const FTAPuzzleResult& Result);
};
