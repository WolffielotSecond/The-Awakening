#pragma once

#include "CoreMinimal.h"
#include "TAPathPuzzleTypes.generated.h"

class UTAItemDefinition;
class UGameplayEffect;

UENUM(BlueprintType)
enum class ETAPuzzleDifficulty : uint8 { Easy, Medium, Hard };

UENUM(BlueprintType)
enum class ETAPuzzleState : uint8 { Ready, Playing, Succeeded, Failed, Aborted };

UENUM(BlueprintType)
enum class ETAPuzzleFailure : uint8 { None, EnergyLimit, UnitLimit, TimeExpired };

/** Signed amounts: positive adds, negative subtracts. Item amounts must be positive. */
UENUM(BlueprintType)
enum class ETAPuzzleEffectType : uint8
{
	None, EnergyLimit, UnitLimit, EnergyUsed, Money, MaxHealth, Health, Item, GameplayEffect
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleEffect
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETAPuzzleEffectType Type = ETAPuzzleEffectType::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Amount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UTAItemDefinition> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UGameplayEffect> GameplayEffectClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float EffectLevel = 1.f;
	bool IsMainGame() const { return Type >= ETAPuzzleEffectType::Money; }
	bool IsPenalty() const;
	FText GetLabel() const;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Column = 0;
	/** Normalized position inside the board, independent of viewport size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FTAPuzzleEffect Effect;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleEdge
{
	GENERATED_BODY()
	/** Node array indices; edges are bidirectional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 From = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 To = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Energy = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FTAPuzzleEffect Effect;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTAPuzzleNode> Nodes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTAPuzzleEdge> Edges;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StartNode = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EndNode = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxEnergy = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxUnits = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimeLimit = 20.f;
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) bool bUsedFallback = false;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETAPuzzleDifficulty Difficulty = ETAPuzzleDifficulty::Medium;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableMinigameEffects = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableMainGameEffects = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableUndo = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UndoAllowance = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableRetry = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 RetryAllowance = 0;
	/** Zero selects a generated 15-30 second limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float TimeLimitOverride = 0.f;
	/** Optional main-game pool. Empty uses money/health defaults, never an undefined item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTAPuzzleEffect> MainGameEffectPool;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<int32> Path;
	UPROPERTY(BlueprintReadOnly) int32 EnergyUsed = 0;
	UPROPERTY(BlueprintReadOnly) int32 UnitsUsed = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxEnergy = 100;
	UPROPERTY(BlueprintReadOnly) int32 MaxUnits = 5;
	UPROPERTY(BlueprintReadOnly) TArray<FTAPuzzleEffect> CollectedEffects;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly) ETAPuzzleFailure Failure = ETAPuzzleFailure::None;
	UPROPERTY(BlueprintReadOnly) FTAPuzzleProgress Progress;
	UPROPERTY(BlueprintReadOnly) TArray<FTAPuzzleEffect> MainGameEffects;
	UPROPERTY(BlueprintReadOnly) float TimeRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
};
