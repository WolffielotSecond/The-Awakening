#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Story/TAWorldEventDefinition.h"
#include "TAInteractionBehaviorComponent.generated.h"
class UTASpecialActionDefinition;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAInteractionAttempt, AActor*, Interactor);
/** Optional configuration on existing ITAInteractable actors. No trace, input, or UI ownership. */
UCLASS(ClassGroup=(TheAwakening), meta=(BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAInteractionBehaviorComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") FTAInteractionConditions Conditions;
 /** Leave a locked door selectable to provide a denial response. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") bool bAllowDeniedAttempt = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") FString DeniedPromptTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") FName EventId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") TObjectPtr<UTASpecialActionDefinition> SpecialAction;
 /** Off by default: adding this component only for conditions must not replace existing pickup/dialogue behavior. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") bool bExecuteConfiguredBehavior = false;
 UPROPERTY(BlueprintAssignable, Category="Interaction") FTAInteractionAttempt OnAllowed;
 UPROPERTY(BlueprintAssignable, Category="Interaction") FTAInteractionAttempt OnDenied;
 UFUNCTION(BlueprintPure, Category="Interaction") bool MeetsConditions(AActor* Interactor) const;
 UFUNCTION(BlueprintPure, Category="Interaction") bool IsSelectable(AActor* Interactor) const;
 UFUNCTION(BlueprintCallable, Category="Interaction") bool TryExecute(AActor* Interactor);
};
