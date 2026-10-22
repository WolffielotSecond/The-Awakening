#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TAQTESequenceComponent.generated.h"
class UTASpecialActionDefinition;
class UTASpecialActionComponent;
class UInputAction;
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FTAQTEStepCallback, int32, StepIndex, UInputAction*, Action, float, Progress);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTAQTEFinishedCallback, bool, bSucceeded);
UENUM(BlueprintType)
enum class ETAQTEKeyGroup : uint8 { Interaction, Direction };
UENUM(BlueprintType)
enum class ETAQTEDirection : uint8 { Up, Down, Left, Right, Random };
UENUM(BlueprintType)
enum class ETAQTEPressMode : uint8 { SinglePress, Mash };
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAQTESequenceStep
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) ETAQTEKeyGroup KeyGroup = ETAQTEKeyGroup::Interaction;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="KeyGroup == ETAQTEKeyGroup::Direction")) ETAQTEDirection Direction = ETAQTEDirection::Right;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) ETAQTEPressMode PressMode = ETAQTEPressMode::Mash;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.001", ClampMax="1", EditCondition="PressMode == ETAQTEPressMode::Mash")) float ProgressPerClick = .15f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1", EditCondition="PressMode == ETAQTEPressMode::Mash")) float DecayPerSecond = .05f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="s", EditCondition="PressMode == ETAQTEPressMode::Mash")) float ClickCooldownSeconds = .1f;
 /** Runtime delegates are supplied by Make QTE Step's Create Event pins. */
 UPROPERTY(Transient) FTAQTEStepCallback OnAcceptedInput;
 UPROPERTY(Transient) FTAQTEStepCallback OnCompleted;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAQTESequenceEnded, bool, bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTAQTESequenceProgress, int32, StepIndex, float, Progress);
/** Scene-side configuration; execution, input and prompts remain on the existing special action component. */
UCLASS(ClassGroup=(TheAwakening), meta=(BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAQTESequenceComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTAQTESequenceComponent();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE") TArray<FTAQTESequenceStep> Steps;
 /** Supplies the IMC, interaction action pool, presentation and completion story event. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE") TSoftObjectPtr<UTASpecialActionDefinition> ActionTemplate;
 /** Ordered Up, Down, Left, Right; action references, never physical keys. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE|Input") TArray<TSoftObjectPtr<UInputAction>> DirectionActions;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE", meta=(ClampMin="0.05", Units="s")) float KeyFlipDuration = .2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE", meta=(ClampMin="0", Units="s")) float ExitInputBufferSeconds = .2f;
 UFUNCTION(BlueprintCallable, Category="QTE") bool StartQTE(AActor* Interactor);
 /** Build and start a private sequence. Array length determines the number of steps. */
 UFUNCTION(BlueprintCallable, Category="QTE", meta=(AutoCreateRefTerm="OnFinished", DisplayName="Generate QTE"))
 bool GenerateQTE(AActor* Interactor, const TArray<FTAQTESequenceStep>& InSteps, const FTAQTEFinishedCallback& OnFinished);
 /** OnAcceptedInput fires per accepted press; OnCompleted fires once when that step succeeds. */
 UFUNCTION(BlueprintPure, Category="QTE", meta=(AutoCreateRefTerm="OnAcceptedInput,OnCompleted", AdvancedDisplay="ProgressPerClick,DecayPerSecond,ClickCooldownSeconds", DisplayName="Make QTE Step"))
 static FTAQTESequenceStep MakeQTEStep(ETAQTEKeyGroup KeyGroup, ETAQTEDirection Direction, ETAQTEPressMode PressMode,
  const FTAQTEStepCallback& OnAcceptedInput, const FTAQTEStepCallback& OnCompleted,
  float ProgressPerClick = .15f, float DecayPerSecond = .05f, float ClickCooldownSeconds = .1f);
 UFUNCTION(BlueprintCallable, Category="QTE") void CancelQTE();
 /** Normalized current mash progress. No external setter. */
 UFUNCTION(BlueprintPure, Category="QTE") float GetMashProgress() const;
 UFUNCTION(BlueprintPure, Category="QTE") int32 GetCurrentStepIndex() const;
 UFUNCTION(BlueprintPure, Category="QTE") bool IsQTEActive() const;
 UPROPERTY(BlueprintAssignable, Category="QTE") FTAQTESequenceEnded OnQTEEnded;
 UPROPERTY(BlueprintAssignable, Category="QTE") FTAQTESequenceProgress OnMashProgressChanged;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 TWeakObjectPtr<UTASpecialActionComponent> RunningAction;
 UPROPERTY(Transient) TArray<FTAQTESequenceStep> RunningSteps;
 UPROPERTY(Transient) FTAQTEFinishedCallback FinishedCallback;
 void HandleAcceptedInput(int32 StepIndex, UInputAction* Action, float Progress);
 void HandleStepCompleted(int32 StepIndex, UInputAction* Action, float Progress);
 UFUNCTION() void HandleUpdated();
 UFUNCTION() void HandleEnded(AActor* Target, bool bSucceeded);
};
