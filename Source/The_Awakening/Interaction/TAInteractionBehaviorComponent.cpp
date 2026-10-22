#include "Interaction/TAInteractionBehaviorComponent.h"
#include "Interaction/TASpecialActionComponent.h"
#include "Story/TADialogueSubsystem.h"
#include "Story/TAWorldEventDirector.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
bool UTAInteractionBehaviorComponent::MeetsConditions(AActor* Interactor) const
{
 const auto* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
 return IsValid(Interactor) && Conditions.Evaluate(GI ? GI->GetSubsystem<UTADialogueSubsystem>() : nullptr, Interactor);
}
bool UTAInteractionBehaviorComponent::IsSelectable(AActor* Interactor) const
{
 return IsValid(Interactor) && (bAllowDeniedAttempt || MeetsConditions(Interactor));
}
bool UTAInteractionBehaviorComponent::TryExecute(AActor* Interactor)
{
 if (!MeetsConditions(Interactor)) { OnDenied.Broadcast(Interactor); return false; }
 if (SpecialAction)
 {
  auto* Actions = Interactor->FindComponentByClass<UTASpecialActionComponent>();
  if (!Actions || !Actions->Start(SpecialAction, GetOwner())) return false;
 }
 else if (!EventId.IsNone())
 {
  bool bStarted = false;
  for (TActorIterator<ATAWorldEventDirector> It(GetWorld()); It; ++It)
   if (It->TryStart(EventId, Interactor)) { bStarted = true; break; }
  if (!bStarted) { OnDenied.Broadcast(Interactor); return false; }
 }
 OnAllowed.Broadcast(Interactor);
 return true;
}
