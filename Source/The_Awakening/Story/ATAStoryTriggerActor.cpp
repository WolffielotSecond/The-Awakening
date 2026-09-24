// Source/The_Awakening/Story/ATAStoryTriggerActor.cpp
#include "Story/ATAStoryTriggerActor.h"
#include "Story/TADialogueSubsystem.h"
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

	// 直接采用项目现有提示控件，触发器实例不需要再手动指定 Widget Class。
	static ConstructorHelpers::FClassFinder<UUserWidget> PromptWidgetClass(
		TEXT("/Game/UI/WBP_InteractPrompt"));
	if (PromptWidgetClass.Succeeded() && InteractPromptComponent)
	{
		InteractPromptComponent->SetWidgetClass(PromptWidgetClass.Class);
	}
}

void ATAStoryTriggerActor::OnInteract_Implementation(AActor* Interactor)
{
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
