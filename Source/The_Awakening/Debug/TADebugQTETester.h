#pragma once
#include "CoreMinimal.h"
#include "Interaction/TAInteractableActor.h"
#include "TADebugQTETester.generated.h"
class UTASpecialActionDefinition;
class UTAQTESequenceComponent;
class UTASpecialActionComponent;
class UTextRenderComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAQTETestEnded, bool, bSucceeded);
UCLASS(Blueprintable, meta=(DisplayName="TA_Debug_QTE_Tester"))
class THE_AWAKENING_API ATADebugQTETester : public ATAInteractableActor
{
 GENERATED_BODY()
public:
 ATADebugQTETester();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="QTE") TObjectPtr<UTAQTESequenceComponent> QTESequence;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="QTE") bool bLastSucceeded = false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="QTE") int32 SuccessfulRuns = 0;
 UPROPERTY(BlueprintAssignable, Category="QTE") FTAQTETestEnded OnTestEnded;
 virtual void OnInteract_Implementation(AActor* Interactor) override;
 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
 TWeakObjectPtr<UTASpecialActionComponent> RunningAction;
 UFUNCTION() void HandleEnded(AActor* Target, bool bSuccess);
};
