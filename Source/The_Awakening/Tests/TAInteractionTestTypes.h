#pragma once
#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "Interaction/TAQTESequenceComponent.h"
#include "TAInteractionTestTypes.generated.h"
/** Isolated automation surface, no live viewport or keyboard dependency. */
UCLASS(Transient, NotBlueprintable)
class UTAInteractionTestLocalPlayer : public ULocalPlayer
{
 GENERATED_BODY()
public:
 int32 PromptRefreshes = 0;
 int32 QTEEndCount = 0;
 bool bLastQTESucceeded = false;
 TArray<int32> AcceptedSteps;
 TArray<int32> CompletedSteps;
 TArray<float> AcceptedProgress;
 TWeakObjectPtr<UTAQTESequenceComponent> CallbackSequence;
 bool bCancelOnAccepted = false;
 bool bCancelOnCompleted = false;
 UFUNCTION() void QTEAccepted(int32 StepIndex, UInputAction* Action, float Progress)
 {
  AcceptedSteps.Add(StepIndex); AcceptedProgress.Add(Progress);
  if (bCancelOnAccepted && CallbackSequence.IsValid()) CallbackSequence->CancelQTE();
 }
 UFUNCTION() void QTECompleted(int32 StepIndex, UInputAction* Action, float Progress)
 {
  CompletedSteps.Add(StepIndex);
  if (bCancelOnCompleted && CallbackSequence.IsValid()) CallbackSequence->CancelQTE();
 }
 UFUNCTION() void QTEEnded(bool bSucceeded) { ++QTEEndCount; bLastQTESucceeded = bSucceeded; }
 UFUNCTION() void PromptChanged() { ++PromptRefreshes; }
 TWeakObjectPtr<UWorld> TestWorld;
 virtual UWorld* GetWorld() const override { return TestWorld.Get(); }
};
