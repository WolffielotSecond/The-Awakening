#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Story/TADialogueTypes.h"
#include "TAWorldEventDefinition.generated.h"

UENUM(BlueprintType)
enum class ETAWorldEventResult : uint8 { Started, Completed, Cancelled, Failed };

/** Conditions share the dialogue evaluators (items, flags, documents, money, attributes). */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAInteractionConditions
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer RequiredTags;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer BlockedTags;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTAStoryCondition> Conditions;
 bool Evaluate(const class UTADialogueSubsystem* Story, AActor* Interactor) const;
};

UCLASS(BlueprintType)
class THE_AWAKENING_API UTAWorldEventDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EventId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bOnce = true;
 /** Shared group reserves a branch while running, commits it only on completion. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ExclusiveGroup;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FTAInteractionConditions Preconditions;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer CompletionTags;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer RemoveTags;
 /** Deltas applied through existing rollback-aware dialogue flags, e.g. Morality. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FString, int32> CompletionFlagDeltas;
};
