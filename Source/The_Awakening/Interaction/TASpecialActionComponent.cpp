#include "Interaction/TASpecialActionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Document/TACharacterActionComponent.h"
#include "Health/TAHealthComponent.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TATimeSubsystem.h"
#include "Story/TAWorldEventDirector.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"

UTASpecialActionComponent::UTASpecialActionComponent()
{
 PrimaryComponentTick.bCanEverTick = true;
 ComponentTags.AddUnique(TEXT("FreezeExempt"));
}
void UTASpecialActionComponent::BeginPlay()
{
 Super::BeginPlay();
 if (auto* Health = GetOwner()->FindComponentByClass<UTAHealthComponent>())
 {
  Health->OnDeathStarted.AddDynamic(this, &UTASpecialActionComponent::OwnerUnavailable);
  Health->OnRespawned.AddDynamic(this, &UTASpecialActionComponent::OwnerUnavailable);
 }
}
bool UTASpecialActionComponent::HasInputPermission() const
{
 const auto* PC = Controller.Get();
 return PC && PC->AllowsInputFor(InputHandle, this, ETAInputCapability::SpecialAction);
}
bool UTASpecialActionComponent::Start(UTASpecialActionDefinition* D, AActor* InTarget)
{
 auto* Character = Cast<AThe_AwakeningCharacter>(GetOwner());
 auto* PC = Character ? Cast<AThe_AwakeningPlayerController>(Character->GetController()) : nullptr;
 auto* Input = Character ? Cast<UEnhancedInputComponent>(Character->InputComponent) : nullptr;
 auto* LocalInput = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
 if (bFinishing || IsExitBufferActive() || IsSpecialActionActive() || !IsValid(D) || D->Stages.IsEmpty() || !IsValid(InTarget) || !Input || !LocalInput ||
     !PC->AllowsInput(ETAInputCapability::Interact) || (!D->bUseUnpausedRealTime && UTAFreezeComponent::IsActorFrozen(Character)) ||
     Character->GetHealthComponent()->IsDead() || Character->GetCharacterActionComponent()->IsActionActive() ||
     FVector::Dist(Character->GetActorLocation(), InTarget->GetActorLocation()) > D->MaxDistance) return false;
 for (const auto& S : D->Stages)
 {
  if (S.Required <= 0 || S.Timeout < 0 || S.ProgressPerPress < 0 || S.DecayPerSecond < 0 || S.ClickCooldownSeconds < 0 || S.PromptOffset.ContainsNaN() ||
      (S.Kind == ETAQTEKind::Animation ? !S.AnimationTag.IsValid() : (!S.Action && S.ActionOptions.IsEmpty()))) return false;
  for (const auto& Option : S.ActionOptions) if (!Option) return false;
  if (S.Kind != ETAQTEKind::Animation && (S.InputEvent == ETriggerEvent::None || S.InputEvent == ETriggerEvent::Completed || S.InputEvent == ETriggerEvent::Canceled)) return false;
  if (S.Kind == ETAQTEKind::Hold && S.InputEvent != ETriggerEvent::Triggered) return false;
  if (S.Kind == ETAQTEKind::Direction)
  {
   if (S.InputEvent != ETriggerEvent::Triggered || S.Direction.IsNearlyZero()) return false;
   if (S.ActionOptions.IsEmpty()) { if (!S.Action || S.Action->ValueType != EInputActionValueType::Axis2D) return false; }
   else for (UInputAction* Option : S.ActionOptions) if (Option->ValueType != EInputActionValueType::Axis2D) return false;
  }
 }
 // Never remove an IMC owned by another system, or borrow its lifetime.
 if (D->MappingContext && LocalInput->HasMappingContext(D->MappingContext)) return false;
 if (D->bAlign)
 {
  const FTransform Alignment = D->RelativeAlignment * InTarget->GetActorTransform();
  const FTransform BeforeAlignment = Character->GetActorTransform();
  FHitResult Hit;
  Character->SetActorLocation(Alignment.GetLocation(), true, &Hit);
  if (Hit.bBlockingHit || !Character->GetActorLocation().Equals(Alignment.GetLocation(), 2.f))
  { Character->SetActorTransform(BeforeAlignment, false, nullptr, ETeleportType::TeleportPhysics); return false; }
  Character->SetActorRotation(Alignment.GetRotation());
 }
 Character->StopCurrentMovement();
 Definition = D; Target = InTarget; Controller = PC; BoundInput = Input; InputSubsystem = LocalInput;
 StageIndex = 0; PresentationTime = 0; bSuspended = false; ++Generation;
 SelectedActions.Reset();
 FRandomStream Random(RandomSeed < 0 ? FMath::Rand() : RandomSeed);
 for (const auto& S : D->Stages)
  SelectedActions.Add(S.ActionOptions.IsEmpty() ? S.Action.Get() : S.ActionOptions[Random.RandRange(0, S.ActionOptions.Num()-1)].Get());
 InTarget->OnDestroyed.AddDynamic(this, &UTASpecialActionComponent::TargetDestroyed);
 FTAInputRequest Request;
 Request.Owner = this; Request.Priority = 75;
 Request.Allowed = {ETAInputCapability::SpecialAction, ETAInputCapability::Look, ETAInputCapability::Pause};
 InputHandle = PC->AcquireInputRequest(Request);
 if (!HasInputPermission()) { Finish(false); return false; }
 PC->OnInputOwnerChanged.AddUObject(this, &UTASpecialActionComponent::HandleOwnershipChange);
 PC->OnPlayerInputOwnershipLost.AddUObject(this, &UTASpecialActionComponent::OwnerUnavailable);
 if (D->MappingContext)
 {
  FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease = true;
  LocalInput->AddMappingContext(D->MappingContext, D->MappingPriority, Options);
  bAddedContext = true;
 }
 TSet<UInputAction*> Actions;
 for (UInputAction* Action : SelectedActions) if (Action) Actions.Add(Action);
 for (auto* Action : Actions)
  for (auto Event : {ETriggerEvent::Started, ETriggerEvent::Triggered, ETriggerEvent::Completed, ETriggerEvent::Canceled})
   BindingHandles.Add(Input->BindAction(Action, Event, this, &UTASpecialActionComponent::HandleInput).GetHandle());
 EnterStage();
 return true;
}
void UTASpecialActionComponent::HandleInput(const FInputActionInstance& Instance)
{
 if (!IsSpecialActionActive() || bSuspended || !HasInputPermission()) return;
 if (const auto* Time = UTATimeSubsystem::Get(this); Time && (Time->HasPauseRequest() || GetWorld()->IsPaused())) return;
 if (!Definition->bUseUnpausedRealTime && UTAFreezeComponent::IsActorFrozen(GetOwner())) return;
 const auto& S = Definition->Stages[StageIndex];
 if (Instance.GetSourceAction() != GetPromptAction() || S.Kind == ETAQTEKind::Animation) return;
 if (Instance.GetTriggerEvent() == ETriggerEvent::Canceled || Instance.GetTriggerEvent() == ETriggerEvent::Completed)
 {
  bAwaitRelease = false;
  if (S.Kind == ETAQTEKind::Hold) { Progress = 0; OnUpdated.Broadcast(); }
  return;
 }
 if (bAwaitRelease || StageTime < StageInputDelay || Instance.GetTriggerEvent() != S.InputEvent) return;
 if (S.Kind == ETAQTEKind::Mash)
 {
  if (StageTime - LastAcceptedClickTime + KINDA_SMALL_NUMBER < S.ClickCooldownSeconds) return;
  LastAcceptedClickTime = StageTime;
 }
 if (S.Kind == ETAQTEKind::Direction)
 {
  if (FVector2D::DotProduct(Instance.GetValue().Get<FVector2D>(), S.Direction.GetSafeNormal()) < S.DirectionThreshold) return;
 }
 // Hold uses EI elapsed trigger time (no physical-key polling). It resets on release.
 if (S.Kind == ETAQTEKind::Hold) Progress = Instance.GetTriggeredTime() / S.Required;
 else Progress = FMath::Min(1.f, Progress + (S.ProgressPerPress > 0.f ? S.ProgressPerPress : 1.f / S.Required));
 const uint64 InputToken = Generation;
 const int32 InputIndex = StageIndex;
 OnInputAccepted.Broadcast(StageIndex, GetPromptAction(), Progress);
 // Scene callbacks may cancel/destroy this action. Never advance a replacement run.
 if (!IsSpecialActionActive() || InputToken != Generation || InputIndex != StageIndex) return;
 if (Progress >= 1.f)
 {
  const uint64 Token = Generation;
  const int32 Index = StageIndex;
  OnUpdated.Broadcast();
  if (IsSpecialActionActive() && Token == Generation && Index == StageIndex) AdvanceStage();
 }
 else OnUpdated.Broadcast();
}
void UTASpecialActionComponent::EnterStage()
{
 if (!IsSpecialActionActive()) return;
 Progress = 0; StageTime = 0; LastAcceptedClickTime = -FLT_MAX;
 StageInputDelay = Definition->bUseQTEPresentation && StageIndex > 0 ? Definition->KeyFlipDuration : 0.f;
 const auto Stage = Definition->Stages[StageIndex];
 const uint64 Token = Generation;
 const auto* PlayerInput = Controller.IsValid() ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
 bAwaitRelease = GetPromptAction() && PlayerInput && PlayerInput->GetActionValue(GetPromptAction()).IsNonZero();
 OnUpdated.Broadcast();
 if (!IsSpecialActionActive() || Token != Generation || !Definition->Stages.IsValidIndex(StageIndex)) return;
 if (Stage.Kind == ETAQTEKind::Animation)
 {
  auto* Animation = GetOwner()->FindComponentByClass<UTACharacterActionComponent>();
  if (!Animation) { Finish(false); return; }
  const int32 Index = StageIndex;
  Animation->PlayAction(Stage.AnimationTag, [Weak = TWeakObjectPtr<UTASpecialActionComponent>(this), Token, Index](bool bInterrupted)
  {
   if (auto* Self = Weak.Get(); Self && Self->IsSpecialActionActive() && Self->Generation == Token && Self->StageIndex == Index)
   { if (bInterrupted) Self->Finish(false); else Self->AdvanceStage(); }
  });
 }
}
void UTASpecialActionComponent::AdvanceStage()
{
 const uint64 Token = Generation;
 const int32 Index = StageIndex;
 OnStageCompleted.Broadcast(StageIndex, GetPromptAction(), Progress);
 if (!IsSpecialActionActive() || Token != Generation || Index != StageIndex) return;
 ++StageIndex;
 if (StageIndex >= Definition->Stages.Num()) Finish(true); else EnterStage();
}
UInputAction* UTASpecialActionComponent::GetPromptAction() const
{
 return Definition && SelectedActions.IsValidIndex(StageIndex) ? SelectedActions[StageIndex].Get() : nullptr;
}
FString UTASpecialActionComponent::GetPromptTextId() const
{
 return Definition && Definition->Stages.IsValidIndex(StageIndex) ? Definition->Stages[StageIndex].PromptTextId : FString();
}
const FTAQTEStage* UTASpecialActionComponent::GetCurrentStage() const
{
 return Definition && Definition->Stages.IsValidIndex(StageIndex) ? &Definition->Stages[StageIndex] : nullptr;
}
void UTASpecialActionComponent::ResumeFromPause()
{
 bSuspended = false;
 bAwaitRelease = false;
 // UI input must not become a gameplay Started/hold on resume. Keep the same selected IA and progress.
 if (auto* Input = InputSubsystem.Get())
 {
  FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease = true;
  if (Definition->MappingContext && !bAddedContext)
  {
   if (Input->HasMappingContext(Definition->MappingContext)) { Finish(false); return; }
   Input->AddMappingContext(Definition->MappingContext, Definition->MappingPriority, Options);
   bAddedContext = true;
  }
  Input->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
 }
 OnUpdated.Broadcast();
}
void UTASpecialActionComponent::HandleOwnershipChange()
{
 if (!IsSpecialActionActive()) return;
 if (HasInputPermission()) { if (bSuspended) ResumeFromPause(); return; }
 // Pause/settings are a temporary overlay. Other owners still interrupt this action.
 if (Controller.IsValid() && (Controller->GetInputWinner().Request.bSuspendSpecialAction || Controller->IsPauseMenuOpen()))
 {
  bSuspended = true;
  // The QTE IMC must not shadow pause/menu navigation or X/Y confirmation bindings.
  if (bAddedContext) if (auto* Input = InputSubsystem.Get())
  {
   FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease = true;
   Input->RemoveMappingContext(Definition->MappingContext, Options);
   bAddedContext = false;
  }
  OnUpdated.Broadcast();
  return;
 }
 Finish(false);
}
void UTASpecialActionComponent::TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta, Tick, Function);
 const auto* Time = UTATimeSubsystem::Get(this);
 // All unscaled timing comes from TimeSubsystem; no private real-time clock or dilation math.
 const bool bRealTime = Definition ? Definition->bUseUnpausedRealTime : bBufferUsesRealTime;
 if (bRealTime) Delta = Time ? Time->GetUnpausedRealDeltaSeconds() : 0.f;
 if (ExitBufferRemaining > 0.f)
 {
  const auto* Pawn = Cast<APawn>(GetOwner());
  if (!BufferController.IsValid() || !Pawn || Pawn->GetController() != BufferController.Get()) ReleaseExitBuffer();
  else if (!BufferController->IsPauseMenuOpen() && !BufferController->GetInputWinner().Request.bSuspendSpecialAction)
  {
   ExitBufferRemaining = FMath::Max(0.f, ExitBufferRemaining - Delta);
   if (ExitBufferRemaining <= 0.f) ReleaseExitBuffer();
  }
 }
 if (!IsSpecialActionActive()) return;
 const auto* Pawn = Cast<APawn>(GetOwner());
 if (!Target.IsValid() || !Controller.IsValid() || !Pawn || Pawn->GetController() != Controller.Get() ||
     (!Definition->bUseUnpausedRealTime && UTAFreezeComponent::IsActorFrozen(GetOwner())) || FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation()) > Definition->MaxDistance)
 { Finish(false); return; }
 if (bSuspended || (Controller.IsValid() && Controller->GetInputWinner().Request.bSuspendSpecialAction)) return;
 if (Time && (Time->HasPauseRequest() || GetWorld()->IsPaused())) return;
 if (!HasInputPermission()) { Finish(false); return; }
 StageTime += Delta;
 PresentationTime += Delta;
 const auto& Stage = Definition->Stages[StageIndex];
 if (Stage.Kind == ETAQTEKind::Mash && Stage.DecayPerSecond > 0 && Progress > 0)
 {
  Progress = FMath::Max(0.f, Progress - Stage.DecayPerSecond * Delta);
  OnUpdated.Broadcast();
  if (!IsSpecialActionActive()) return;
 }
 const float Timeout = Definition->Stages[StageIndex].Timeout;
 if (Timeout > 0 && StageTime >= Timeout) Finish(false, true);
}
void UTASpecialActionComponent::Finish(bool bSuccess, bool bTimedOut)
{
 if (!IsSpecialActionActive() || bFinishing) return;
 bFinishing = true;
 const int32 FinishedStage = StageIndex;
 UInputAction* FinishedAction = GetPromptAction();
 const float FinishedProgress = Progress;
 const FName Event = Definition->CompletionEvent;
 AActor* FinishedTarget = Target.Get();
 const bool bAnimation = Definition->Stages.IsValidIndex(StageIndex) && Definition->Stages[StageIndex].Kind == ETAQTEKind::Animation;
 auto* OldDefinition = Definition.Get();
 if (auto* PC = Controller.Get())
  for (const auto& Action : SelectedActions) PC->SuppressHeldActionUntilRelease(Action);
 Definition = nullptr; bSuspended = false; SelectedActions.Reset(); ++Generation;
 if (FinishedTarget) FinishedTarget->OnDestroyed.RemoveDynamic(this, &UTASpecialActionComponent::TargetDestroyed);
 Target.Reset();
 if (auto* Input = BoundInput.Get()) for (auto Handle : BindingHandles) Input->RemoveBindingByHandle(Handle);
 BindingHandles.Reset(); BoundInput.Reset();
 if (bAddedContext) if (auto* Subsystem = InputSubsystem.Get())
 {
  FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease = true;
  Subsystem->RemoveMappingContext(OldDefinition->MappingContext, Options);
  Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
 }
 bAddedContext = false; InputSubsystem.Reset();
 if (auto* PC = Controller.Get())
 {
  PC->OnInputOwnerChanged.RemoveAll(this); PC->OnPlayerInputOwnershipLost.RemoveAll(this);
  // Keep the same ownership handle until the buffer expires; no one-frame movement gap.
  if (bSuccess && OldDefinition->bUseQTEPresentation && OldDefinition->ExitInputBufferSeconds > 0.f)
  { BufferController = PC; BufferHandle = InputHandle; ExitBufferRemaining = OldDefinition->ExitInputBufferSeconds; bBufferUsesRealTime = OldDefinition->bUseUnpausedRealTime; }
  else PC->ReleaseInputRequest(InputHandle);
 }
 InputHandle = 0; Controller.Reset();
 if (bAnimation) if (auto* Animation = GetOwner()->FindComponentByClass<UTACharacterActionComponent>()) Animation->InterruptAction();
 Progress = bSuccess ? 1.f : 0.f;
 OnUpdated.Broadcast();
 // Notify timeout only after releasing input and ending execution. Never advance a stage.
 if (bTimedOut) OnStageTimedOut.Broadcast(FinishedStage, FinishedAction, FinishedProgress);
 OnEnded.Broadcast(FinishedTarget, bSuccess);
 bFinishing = false;
 if (bSuccess && !Event.IsNone() && IsValid(GetOwner()))
 {
  bool bSubmitted = false;
  for (TActorIterator<ATAWorldEventDirector> It(GetWorld()); It; ++It)
   if (It->TryStart(Event, GetOwner())) { bSubmitted = true; break; }
  if (!bSubmitted) UE_LOG(LogTemp, Warning, TEXT("Special action completed, but world event '%s' has no eligible director."), *Event.ToString());
 }
}
void UTASpecialActionComponent::ReleaseExitBuffer()
{
 const auto Handle = BufferHandle;
 auto* PC = BufferController.Get();
 BufferHandle = 0; BufferController.Reset(); ExitBufferRemaining = 0.f; bBufferUsesRealTime = false;
 if (PC) PC->ReleaseInputRequest(Handle);
}
void UTASpecialActionComponent::Cancel() { Finish(false); ReleaseExitBuffer(); }
void UTASpecialActionComponent::OwnerUnavailable() { Cancel(); }
void UTASpecialActionComponent::TargetDestroyed(AActor*) { Cancel(); }
void UTASpecialActionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 Cancel();
 if (auto* Health = GetOwner()->FindComponentByClass<UTAHealthComponent>())
 {
  Health->OnDeathStarted.RemoveDynamic(this, &UTASpecialActionComponent::OwnerUnavailable);
  Health->OnRespawned.RemoveDynamic(this, &UTASpecialActionComponent::OwnerUnavailable);
 }
 Super::EndPlay(Reason);
}
