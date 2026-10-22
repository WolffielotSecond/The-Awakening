#include "Story/TAWorldEventDirector.h"
#include "Story/TADialogueSubsystem.h"
#include "Engine/GameInstance.h"
#include "Health/TAHealthComponent.h"
ATAWorldEventDirector::ATAWorldEventDirector() { PrimaryActorTick.bCanEverTick = true; }
void ATAWorldEventDirector::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const auto Snapshot = Participants;
 for (const auto& Pair : Snapshot)
 {
  const auto* Actor = Pair.Value.Get();
  const auto* Health = Actor ? Actor->FindComponentByClass<UTAHealthComponent>() : nullptr;
  if (!Actor || (Health && Health->IsDead())) Finish(Pair.Key, ETAWorldEventResult::Cancelled);
 }
}
void ATAWorldEventDirector::StateChanged() { ApplyPersistentState(); }
void ATAWorldEventDirector::BeginPlay()
{
 Super::BeginPlay();
 GetGameInstance()->GetSubsystem<UTADialogueSubsystem>()->OnWorldStateChanged.AddDynamic(this, &ATAWorldEventDirector::StateChanged);
 ApplyPersistentState();
 GetGameInstance()->GetSubsystem<UTADialogueSubsystem>()->OnWorldEventChanged.AddDynamic(this, &ATAWorldEventDirector::HandleEvent);
}
bool ATAWorldEventDirector::TryStart(FName Id, AActor* Interactor)
{
 if (bEndingPlay) return false;
 auto* Story = GetGameInstance()->GetSubsystem<UTADialogueSubsystem>();
 for (UTAWorldEventDefinition* D : Events) if (D && D->EventId == Id)
 {
  if (!Story->CanStartWorldEvent(D, Interactor)) return false;
  Running.Add(Id);
  Participants.Add(Id, Interactor);
  if (!Story->StartWorldEvent(D, Interactor, this)) { Running.Remove(Id); Participants.Remove(Id); return false; }
  if (Running.Contains(Id)) ExecuteEvent(Id, Interactor);
  return true;
 }
 return false;
}
bool ATAWorldEventDirector::Finish(FName Id, ETAWorldEventResult Result)
{
 return GetGameInstance()->GetSubsystem<UTADialogueSubsystem>()->FinishWorldEvent(Id, Result, this);
}
void ATAWorldEventDirector::HandleEvent(FName Id, ETAWorldEventResult Result, AActor*)
{
 if (Result != ETAWorldEventResult::Started && Running.Remove(Id)) { Participants.Remove(Id); EventEnded(Id, Result); }
}
void ATAWorldEventDirector::EndPlay(const EEndPlayReason::Type Reason)
{
 bEndingPlay = true;
 if (auto* GI = GetGameInstance())
 {
  auto* Story = GI->GetSubsystem<UTADialogueSubsystem>();
  Story->OnWorldStateChanged.RemoveDynamic(this, &ATAWorldEventDirector::StateChanged);
  Story->CancelWorldEventsFor(this);
  Story->OnWorldEventChanged.RemoveDynamic(this, &ATAWorldEventDirector::HandleEvent);
 }
 Super::EndPlay(Reason);
}
