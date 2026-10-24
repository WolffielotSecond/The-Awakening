#include "Interaction/TAQTESequenceComponent.h"
#include "Interaction/TASpecialActionComponent.h"
#include "InputAction.h"

DEFINE_LOG_CATEGORY_STATIC(LogTAQTESequence, Log, All);

UTAQTESequenceComponent::UTAQTESequenceComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
 ActionTemplate = TSoftObjectPtr<UTASpecialActionDefinition>(FSoftObjectPath(TEXT("/Game/DeubgItem/QTE/DA_Debug_QTE_Pipe.DA_Debug_QTE_Pipe")));
 for (const TCHAR* Name : {TEXT("IA_MoveForward"), TEXT("IA_MoveBackward"), TEXT("IA_MoveLeft"), TEXT("IA_MoveRight")})
  DirectionActions.Add(TSoftObjectPtr<UInputAction>(FSoftObjectPath(FString(TEXT("/Game/Input/Actions/")) + Name + TEXT(".") + Name)));
 Steps.AddDefaulted();
 FTAQTESequenceStep Final; Final.KeyGroup = ETAQTEKeyGroup::Direction; Final.PressMode = ETAQTEPressMode::SinglePress;
 Steps.Add(Final);
}
bool UTAQTESequenceComponent::StartQTE(AActor* Interactor)
{
 return GenerateQTE(Interactor, Steps, FTAQTEFinishedCallback());
}
FTAQTESequenceStep UTAQTESequenceComponent::MakeQTEStep(ETAQTEKeyGroup KeyGroup, ETAQTEDirection Direction, ETAQTEPressMode PressMode,
 const FTAQTEStepCallback& OnAcceptedInput, const FTAQTEStepCallback& OnCompleted, const FTAQTEStepCallback& OnTimedOut,
 float ProgressPerClick, float DecayPerSecond, float ClickCooldownSeconds, bool bTimeLimited, float TimeLimitSeconds, FVector2D PromptOffset)
{
 FTAQTESequenceStep Step;
 Step.KeyGroup = KeyGroup; Step.Direction = Direction; Step.PressMode = PressMode;
 Step.ProgressPerClick = ProgressPerClick; Step.DecayPerSecond = DecayPerSecond; Step.ClickCooldownSeconds = ClickCooldownSeconds;
 Step.OnAcceptedInput = OnAcceptedInput; Step.OnCompleted = OnCompleted;
 Step.bTimeLimited = bTimeLimited; Step.TimeLimitSeconds = TimeLimitSeconds; Step.OnTimedOut = OnTimedOut;
 Step.PromptOffset = PromptOffset;
 return Step;
}
bool UTAQTESequenceComponent::GenerateQTE(AActor* Interactor, const TArray<FTAQTESequenceStep>& InSteps, const FTAQTEFinishedCallback& OnFinished)
{
 if (RunningAction.IsValid() || !IsValid(Interactor) || InSteps.IsEmpty()) return false;
 auto* Action = Interactor->FindComponentByClass<UTASpecialActionComponent>();
 if (!Action) return false;
 auto* Source = ActionTemplate.LoadSynchronous();
 if (!Source)
 {
  UE_LOG(LogTAQTESequence, Warning, TEXT("QTE action template unavailable: '%s' on %s. Verify the template and its dependencies are cooked."),
   *ActionTemplate.ToSoftObjectPath().ToString(), *GetNameSafe(GetOwner()));
  return false;
 }
 auto* Runtime = DuplicateObject<UTASpecialActionDefinition>(Source, this);
 TArray<TObjectPtr<UInputAction>> InteractionActions;
 for (const auto& Stage : Source->Stages)
  if (Stage.Kind == ETAQTEKind::Mash || Stage.Kind == ETAQTEKind::Press)
  {
   InteractionActions = Stage.ActionOptions;
   if (InteractionActions.IsEmpty() && Stage.Action) InteractionActions.Add(Stage.Action);
   break;
  }
 Runtime->Stages.Reset(); Runtime->bUseQTEPresentation = true; Runtime->bUseUnpausedRealTime = true;
 Runtime->KeyFlipDuration = KeyFlipDuration; Runtime->ExitInputBufferSeconds = ExitInputBufferSeconds;
 for (const auto& Step : InSteps)
 {
  if (Step.bTimeLimited && (!FMath::IsFinite(Step.TimeLimitSeconds) || Step.TimeLimitSeconds <= 0.f)) return false;
  FTAQTEStage Stage; Stage.Timeout = Step.bTimeLimited ? Step.TimeLimitSeconds : 0.f;
  Stage.Kind = Step.PressMode == ETAQTEPressMode::Mash ? ETAQTEKind::Mash : ETAQTEKind::Press;
  Stage.ProgressPerPress = Step.PressMode == ETAQTEPressMode::Mash ? Step.ProgressPerClick : 1.f;
  Stage.DecayPerSecond = Step.PressMode == ETAQTEPressMode::Mash ? Step.DecayPerSecond : 0.f;
  Stage.ClickCooldownSeconds = Step.ClickCooldownSeconds;
  Stage.PromptOffset = Step.PromptOffset;
  if (Step.KeyGroup == ETAQTEKeyGroup::Interaction) Stage.ActionOptions = InteractionActions;
  else
  {
   const int32 First = Step.Direction == ETAQTEDirection::Random ? 0 : int32(Step.Direction);
   const int32 Last = Step.Direction == ETAQTEDirection::Random ? 3 : First;
   for (int32 Index = First; Index <= Last; ++Index)
   {
    if (!DirectionActions.IsValidIndex(Index)) return false;
    auto* Direction = DirectionActions[Index].LoadSynchronous();
    if (!Direction)
    {
     UE_LOG(LogTAQTESequence, Warning, TEXT("QTE direction action unavailable: '%s' on %s. Verify cooked input assets."),
      *DirectionActions[Index].ToSoftObjectPath().ToString(), *GetNameSafe(GetOwner()));
     return false;
    }
    Stage.ActionOptions.Add(Direction);
   }
  }
  if (Stage.ActionOptions.IsEmpty()) return false;
  Runtime->Stages.Add(Stage);
 }
 RunningAction = Action;
 RunningSteps = InSteps;
 FinishedCallback = OnFinished;
 Action->OnUpdated.AddUniqueDynamic(this, &UTAQTESequenceComponent::HandleUpdated);
 Action->OnEnded.AddUniqueDynamic(this, &UTAQTESequenceComponent::HandleEnded);
 Action->OnInputAccepted.AddUObject(this, &UTAQTESequenceComponent::HandleAcceptedInput);
 Action->OnStageCompleted.AddUObject(this, &UTAQTESequenceComponent::HandleStepCompleted);
 Action->OnStageTimedOut.AddUObject(this, &UTAQTESequenceComponent::HandleStepTimedOut);
 if (Action->Start(Runtime, GetOwner())) return true;
 Action->OnUpdated.RemoveDynamic(this, &UTAQTESequenceComponent::HandleUpdated);
 Action->OnEnded.RemoveDynamic(this, &UTAQTESequenceComponent::HandleEnded);
 Action->OnInputAccepted.RemoveAll(this); Action->OnStageCompleted.RemoveAll(this);
 Action->OnStageTimedOut.RemoveAll(this);
 RunningSteps.Reset(); FinishedCallback.Unbind(); RunningAction.Reset(); return false;
}
void UTAQTESequenceComponent::HandleAcceptedInput(int32 StepIndex, UInputAction* Action, float Progress)
{
 if (!IsQTEActive() || !RunningSteps.IsValidIndex(StepIndex)) return;
 // Execute a copy: the callback is allowed to cancel and clear the runtime sequence.
 const auto Callback = RunningSteps[StepIndex].OnAcceptedInput;
 Callback.ExecuteIfBound(StepIndex, Action, Progress);
}
void UTAQTESequenceComponent::HandleStepCompleted(int32 StepIndex, UInputAction* Action, float Progress)
{
 if (!IsQTEActive() || !RunningSteps.IsValidIndex(StepIndex)) return;
 const auto Callback = RunningSteps[StepIndex].OnCompleted;
 Callback.ExecuteIfBound(StepIndex, Action, Progress);
}
void UTAQTESequenceComponent::HandleStepTimedOut(int32 StepIndex, UInputAction* Action, float Progress)
{
 // The executor has already stopped; retain the run copy until its OnEnded follows.
 if (!RunningAction.IsValid() || !RunningSteps.IsValidIndex(StepIndex)) return;
 const auto Callback = RunningSteps[StepIndex].OnTimedOut;
 OnStepTimedOut.Broadcast(StepIndex, Action, Progress);
 Callback.ExecuteIfBound(StepIndex, Action, Progress);
}
bool UTAQTESequenceComponent::IsQTEActive() const
{ return RunningAction.IsValid() && RunningAction->IsSpecialActionActive() && RunningAction->GetTarget() == GetOwner(); }
int32 UTAQTESequenceComponent::GetCurrentStepIndex() const
{ return IsQTEActive() ? RunningAction->GetStageIndex() : INDEX_NONE; }
float UTAQTESequenceComponent::GetMashProgress() const
{
 const auto* Stage = IsQTEActive() ? RunningAction->GetCurrentStage() : nullptr;
 return Stage && Stage->Kind == ETAQTEKind::Mash ? RunningAction->GetProgress() : 0.f;
}
void UTAQTESequenceComponent::HandleUpdated()
{ if (IsQTEActive()) OnMashProgressChanged.Broadcast(GetCurrentStepIndex(), GetMashProgress()); }
void UTAQTESequenceComponent::HandleEnded(AActor* Target, bool bSucceeded)
{
 if (Target != GetOwner()) return;
 if (auto* Action = RunningAction.Get())
 {
  Action->OnUpdated.RemoveDynamic(this, &UTAQTESequenceComponent::HandleUpdated);
  Action->OnEnded.RemoveDynamic(this, &UTAQTESequenceComponent::HandleEnded);
  Action->OnInputAccepted.RemoveAll(this); Action->OnStageCompleted.RemoveAll(this);
  Action->OnStageTimedOut.RemoveAll(this);
 }
 const auto Callback = FinishedCallback;
 RunningSteps.Reset(); FinishedCallback.Unbind(); RunningAction.Reset();
 OnQTEEnded.Broadcast(bSucceeded);
 Callback.ExecuteIfBound(bSucceeded);
}
void UTAQTESequenceComponent::CancelQTE()
{ if (IsQTEActive()) RunningAction->Cancel(); }
void UTAQTESequenceComponent::EndPlay(const EEndPlayReason::Type Reason)
{ CancelQTE(); Super::EndPlay(Reason); }
