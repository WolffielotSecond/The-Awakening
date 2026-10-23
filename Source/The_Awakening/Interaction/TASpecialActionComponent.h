#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TAInputRouter.h"
#include "Interaction/TASpecialActionDefinition.h"
#include "TASpecialActionComponent.generated.h"
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class AThe_AwakeningPlayerController;
struct FInputActionInstance;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTASpecialActionUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTASpecialActionEnded, AActor*, Target, bool, bSucceeded);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FTASpecialActionStepEvent, int32, UInputAction*, float);
UCLASS(ClassGroup=(TheAwakening), meta=(BlueprintSpawnableComponent))
class THE_AWAKENING_API UTASpecialActionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTASpecialActionComponent();
 UFUNCTION(BlueprintCallable, Category="Interaction|Action") bool Start(UTASpecialActionDefinition* Definition, AActor* Target);
 UFUNCTION(BlueprintCallable, Category="Interaction|Action") void Cancel();
 UFUNCTION(BlueprintPure, Category="Interaction|Action") bool IsSpecialActionActive() const { return Definition != nullptr; }
 UFUNCTION(BlueprintPure, Category="Interaction|Action") float GetProgress() const { return Progress; }
 UFUNCTION(BlueprintPure, Category="Interaction|Action") int32 GetStageIndex() const { return StageIndex; }
 UFUNCTION(BlueprintPure, Category="Interaction|Action") AActor* GetTarget() const { return Target.Get(); }
 UFUNCTION(BlueprintPure, Category="Interaction|Action") UInputAction* GetPromptAction() const;
 UFUNCTION(BlueprintPure, Category="Interaction|Action") FString GetPromptTextId() const;
 bool HasInputPermission() const;
 UFUNCTION(BlueprintPure, Category="Interaction|Action") bool IsSuspended() const { return bSuspended; }
 UFUNCTION(BlueprintPure, Category="Interaction|Action") bool IsExitBufferActive() const { return ExitBufferRemaining > 0.f; }
 const FTAQTEStage* GetCurrentStage() const;
 const UTASpecialActionDefinition* GetDefinition() const { return Definition; }
 float GetStageElapsed() const { return StageTime; }
 float GetPresentationTime() const { return PresentationTime; }
 bool UsesQTEPresentation() const { return Definition && Definition->bUseQTEPresentation; }
 /** Injectable stream for reproducible debug/automation runs; negative restores random seed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Action") int32 RandomSeed = -1;
 UPROPERTY(BlueprintAssignable) FTASpecialActionUpdated OnUpdated;
 UPROPERTY(BlueprintAssignable) FTASpecialActionEnded OnEnded;
 /** Accepted Enhanced Input only; wrong keys, cooldown and suspended input never emit these. */
 FTASpecialActionStepEvent OnInputAccepted;
 FTASpecialActionStepEvent OnStageCompleted;
 FTASpecialActionStepEvent OnStageTimedOut;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
private:
 void HandleInput(const FInputActionInstance& Instance);
 void EnterStage();
 void AdvanceStage();
 void Finish(bool bSuccess, bool bTimedOut = false);
 void ReleaseExitBuffer();
 float ExitBufferRemaining = 0.f;
 bool bBufferUsesRealTime = false;
 TWeakObjectPtr<AThe_AwakeningPlayerController> BufferController;
 FTAInputRouter::FHandle BufferHandle = 0;
 void HandleOwnershipChange();
 UPROPERTY(Transient) TObjectPtr<UTASpecialActionDefinition> Definition;
 TWeakObjectPtr<AActor> Target;
 TWeakObjectPtr<AThe_AwakeningPlayerController> Controller;
 TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
 TArray<uint32> BindingHandles;
 FTAInputRouter::FHandle InputHandle = 0;
 bool bAddedContext = false;
 bool bFinishing = false;
 int32 StageIndex = 0;
 float Progress = 0.f;
 float StageTime = 0.f;
 uint64 Generation = 0;
 bool bAwaitRelease = false;
 bool bSuspended = false;
 float PresentationTime = 0.f;
 float StageInputDelay = 0.f;
 float LastAcceptedClickTime = -FLT_MAX;
 UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> SelectedActions;
 void ResumeFromPause();
 UFUNCTION() void OwnerUnavailable();
 UFUNCTION() void TargetDestroyed(AActor* Actor);
};
