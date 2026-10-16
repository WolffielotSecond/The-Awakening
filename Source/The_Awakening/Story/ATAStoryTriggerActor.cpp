// Source/The_Awakening/Story/ATAStoryTriggerActor.cpp
#include "Story/ATAStoryTriggerActor.h"
#include "Story/TADialogueSubsystem.h"
#include "Health/TARollbackSubsystem.h"
#include "Interaction/TADialogueParticipant.h"
#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ATAStoryTriggerActor::ATAStoryTriggerActor()
{
	InteractTextId = TEXT("Interact_Talk");

	// 独立碰撞体确保没有 Static Mesh 的剧情触发器也能被玩家扫描到。
	TriggerCollision = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerCollision"));
	TriggerCollision->SetupAttachment(RootComponent);
	TriggerCollision->InitSphereRadius(200.f);
	TriggerCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TriggerCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerCollision->SetGenerateOverlapEvents(true);
	// 提示控件不再在这里指定：统一提示锚点会使用项目设置里的默认提示控件。
}

void ATAStoryTriggerActor::OnInteract_Implementation(AActor* Interactor)
{
	// Re-evaluate the same eligibility used by prompts; no duplicate movement rules.
	if (!ITAInteractable::Execute_CanInteract(this, Interactor)) return;
	if (!StoryAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 触发器 %s 未指定 Story Asset"), *GetName());
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

	if (Subsystem->StartDialogueAsset(StoryAsset, this))
	{
		const bool bPlayedBefore = bPlayed;
		bPlayed = true;
		// 对话中死亡回滚时恢复“未播放”，让触发器可以再次触发
		if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this); Rollback && Rollback->IsRecording())
		{
			Rollback->RecordUndo(TEXT("StoryTrigger.Played"), [WeakThis = TWeakObjectPtr<ATAStoryTriggerActor>(this), bPlayedBefore]
			{
				if (ATAStoryTriggerActor* Trigger = WeakThis.Get()) Trigger->bPlayed = bPlayedBefore;
			});
		}
	}
}

bool ATAStoryTriggerActor::CanInteract_Implementation(AActor* Interactor) const
{
	if (!IsValid(Interactor) || !Interactor->Implements<UTADialogueParticipant>() ||
		!ITADialogueParticipant::Execute_CanParticipateInDialogue(Interactor)) return false;
	if (bTriggerOnce && bPlayed)
	{
		return false;
	}
	return Super::CanInteract_Implementation(Interactor);
}
