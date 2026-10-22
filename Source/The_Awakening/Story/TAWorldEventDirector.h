#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Story/TAWorldEventDefinition.h"
#include "TAWorldEventDirector.generated.h"

/** Level-local coordinator. No player input or target detection here. */
UCLASS(Blueprintable)
class THE_AWAKENING_API ATAWorldEventDirector : public AActor
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story") TArray<TObjectPtr<UTAWorldEventDefinition>> Events;
 UFUNCTION(BlueprintCallable, Category="Story") bool TryStart(FName EventId, AActor* Interactor);
 UFUNCTION(BlueprintCallable, Category="Story") bool Finish(FName EventId, ETAWorldEventResult Result);
 UFUNCTION(BlueprintImplementableEvent, Category="Story") void ExecuteEvent(FName EventId, AActor* Interactor);
 UFUNCTION(BlueprintImplementableEvent, Category="Story") void ApplyPersistentState();
 UFUNCTION(BlueprintImplementableEvent, Category="Story") void EventEnded(FName EventId, ETAWorldEventResult Result);
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 ATAWorldEventDirector();
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void StateChanged();
 UFUNCTION() void HandleEvent(FName Id, ETAWorldEventResult Result, AActor* Interactor);
 bool bEndingPlay = false;
 TSet<FName> Running;
 TMap<FName, TWeakObjectPtr<AActor>> Participants;
};
