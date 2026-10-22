#include "Story/TADialogueSubsystem.h"
#include "Inventory/TAInventoryComponent.h"
#include "Core/TAPlayerState.h"
#include "GameFramework/Pawn.h"

bool FTAInteractionConditions::Evaluate(const UTADialogueSubsystem* Story, AActor* Interactor) const
{
 if (!Story) return RequiredTags.IsEmpty() && BlockedTags.IsEmpty() && Conditions.IsEmpty();
 const auto Tags = Story->GetWorldStateTags();
 if (!Tags.HasAll(RequiredTags) || Tags.HasAny(BlockedTags)) return false;
 FTADialogueConditionContext Context;
 Context.Dialogue = Story;
 Context.World = Interactor ? Interactor->GetWorld() : Story->GetWorld();
 Context.Inventory = Interactor ? Interactor->FindComponentByClass<UTAInventoryComponent>() : nullptr;
 if (const APawn* Pawn = Cast<APawn>(Interactor)) Context.PlayerState = Pawn->GetPlayerState<ATAPlayerState>();
 for (const auto& Condition : Conditions) if (!Story->EvaluateCondition(Condition, Context)) return false;
 return true;
}
void UTADialogueSubsystem::RecordWorldStateUndo()
{
 if (auto* Rollback = UTARollbackSubsystem::Get(this); Rollback && Rollback->IsRecording())
 {
  Rollback->RecordUndo(TEXT("Story.WorldState"), [Weak = TWeakObjectPtr<UTADialogueSubsystem>(this), Tags = WorldStateTags, Completed = CompletedWorldEvents, Branches = WorldBranches]
  {
   if (auto* Self = Weak.Get())
   {
    Self->WorldStateTags = Tags; Self->CompletedWorldEvents = Completed; Self->WorldBranches = Branches;
    Self->OnWorldStateChanged.Broadcast();
   }
  });
 }
}
void UTADialogueSubsystem::SetWorldStateTag(FGameplayTag Tag, bool bEnabled)
{
 if (!Tag.IsValid() || WorldStateTags.HasTagExact(Tag) == bEnabled) return;
 RecordWorldStateUndo();
 if (bEnabled) WorldStateTags.AddTag(Tag); else WorldStateTags.RemoveTag(Tag);
 OnWorldStateChanged.Broadcast();
}
bool UTADialogueSubsystem::CanStartWorldEvent(UTAWorldEventDefinition* D, AActor* Interactor) const
{
 if (bCancellingWorldEvents || !IsValid(D) || D->EventId.IsNone() || ActiveWorldEvents.Contains(D->EventId) ||
     (D->bOnce && CompletedWorldEvents.Contains(D->EventId)) || !D->Preconditions.Evaluate(this, Interactor)) return false;
 if (!D->ExclusiveGroup.IsNone())
 {
  if (WorldBranches.Contains(D->ExclusiveGroup)) return false;
  for (const auto& Pair : ActiveWorldEvents)
   if (const auto* Other = Pair.Value.Definition.Get(); Other && Other->ExclusiveGroup == D->ExclusiveGroup) return false;
 }
 return true;
}
bool UTADialogueSubsystem::StartWorldEvent(UTAWorldEventDefinition* D, AActor* Interactor, UObject* Executor)
{
 if (!IsValid(Executor) || !IsValid(Interactor) || !CanStartWorldEvent(D, Interactor)) return false;
 ActiveWorldEvents.Add(D->EventId, {D, Interactor, Executor});
 OnWorldEventChanged.Broadcast(D->EventId, ETAWorldEventResult::Started, Interactor);
 return true;
}
bool UTADialogueSubsystem::FinishWorldEvent(FName Id, ETAWorldEventResult Result, UObject* Executor)
{
 const auto* Entry = ActiveWorldEvents.Find(Id);
 if (!Entry || Entry->Executor.Get() != Executor || Result == ETAWorldEventResult::Started) return false;
 const auto Snapshot = *Entry;
 ActiveWorldEvents.Remove(Id); // Reentrant listeners see final state.
 const auto* D = Snapshot.Definition.Get();
 if (Result == ETAWorldEventResult::Completed && D)
 {
  RecordWorldStateUndo();
  CompletedWorldEvents.Add(Id);
  if (!D->ExclusiveGroup.IsNone()) WorldBranches.Add(D->ExclusiveGroup, Id);
  WorldStateTags.RemoveTags(D->RemoveTags);
  WorldStateTags.AppendTags(D->CompletionTags);
  for (const auto& Delta : D->CompletionFlagDeltas) SetFlagInt(Delta.Key, GetFlagInt(Delta.Key) + Delta.Value);
 }
 OnWorldStateChanged.Broadcast();
 OnWorldEventChanged.Broadcast(Id, Result, Snapshot.Interactor.Get());
 // Same event bus used by dialogue/level listeners, with an explicit lifecycle payload.
 BroadcastStoryEvent(Id, {{TEXT("Result"), UEnum::GetValueAsString(Result)}});
 return true;
}
void UTADialogueSubsystem::CancelWorldEventsFor(UObject* Executor)
{
 TArray<FName> Ids;
 for (const auto& Pair : ActiveWorldEvents) if (Pair.Value.Executor.Get() == Executor) Ids.Add(Pair.Key);
 for (FName Id : Ids) FinishWorldEvent(Id, ETAWorldEventResult::Cancelled, Executor);
}
void UTADialogueSubsystem::CancelAllWorldEvents()
{
 TGuardValue<bool> Guard(bCancellingWorldEvents, true);
 const auto Snapshot = ActiveWorldEvents;
 for (const auto& Pair : Snapshot) FinishWorldEvent(Pair.Key, ETAWorldEventResult::Cancelled, Pair.Value.Executor.Get());
}
