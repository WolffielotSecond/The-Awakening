// Source/The_Awakening/Story/ATAStoryTriggerActor.cpp
#include "Story/ATAStoryTriggerActor.h"
#include "Story/TADialogueSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

ATAStoryTriggerActor::ATAStoryTriggerActor()
{
	InteractTextId = TEXT("Interact_Talk");
}

void ATAStoryTriggerActor::OnInteract_Implementation(AActor* Interactor)
{
	if (StoryId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 触发器 %s 未设置 StoryId"), *GetName());
		return;
	}

	UGameInstance* GI = Interactor ? Interactor->GetGameInstance() : GetGameInstance();
	if (!GI)
	{
		return;
	}

	UTADialogueSubsystem* Subsystem = GI->GetSubsystem<UTADialogueSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (Subsystem->IsDialogueActive())
	{
		return;
	}

	if (Subsystem->StartDialogue(StoryId, this))
	{
		bPlayed = true;
	}
}

bool ATAStoryTriggerActor::CanInteract_Implementation(AActor* Interactor) const
{
	if (bTriggerOnce && bPlayed)
	{
		return false;
	}
	return Super::CanInteract_Implementation(Interactor);
}
