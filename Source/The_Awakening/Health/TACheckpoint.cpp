#include "Health/TACheckpoint.h"
#include "Health/TARespawnSubsystem.h"
#include "Health/TAHealthComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

ATACheckpoint::ATACheckpoint()
{
	InteractTextId = TEXT("Interact_Checkpoint");

	InteractionRange = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionRange"));
	InteractionRange->SetupAttachment(RootComponent);
	InteractionRange->InitBoxExtent(FVector(150.f, 150.f, 100.f));
	InteractionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	// 与剧情触发器一致：Visibility 供角色交互查询发现，Pawn 重叠供范围判断与自动登记
	InteractionRange->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionRange->SetGenerateOverlapEvents(true);

	SpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(RootComponent);
	SpawnPoint->SetRelativeLocation(FVector(200.f, 0.f, 100.f));
	SpawnPoint->ArrowColor = FColor::Green;
}

FTransform ATACheckpoint::GetSpawnTransform() const
{
	return SpawnPoint ? SpawnPoint->GetComponentTransform() : GetActorTransform();
}

void ATACheckpoint::BeginPlay()
{
	Super::BeginPlay();
	if (InteractionRange)
	{
		InteractionRange->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATACheckpoint::HandleRangeBeginOverlap);
	}
	if (UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(this))
	{
		Respawn->RegisterCheckpoint(this);
	}
}

void ATACheckpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(this))
	{
		Respawn->UnregisterCheckpoint(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ATACheckpoint::SetActiveCheckpoint(bool bInActive)
{
	if (bActive == bInActive)
	{
		return;
	}
	bActive = bInActive;
	if (bActive) OnActivated();
	else OnDeactivated();
}

bool ATACheckpoint::CanInteract_Implementation(AActor* Interactor) const
{
	// 提示条件与执行条件一致：只有能复活的角色（有生命组件）站在交互范围内才能登记
	return IsValid(Interactor) && UTAHealthComponent::FindHealthComponent(Interactor) &&
		InteractionRange && InteractionRange->IsOverlappingActor(Interactor);
}

void ATACheckpoint::OnInteract_Implementation(AActor* Interactor)
{
	if (UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(this))
	{
		Respawn->ActivateCheckpoint(this);
	}
}

void ATACheckpoint::HandleRangeBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bActivateOnOverlap && Pawn && Pawn->IsPlayerControlled() && UTAHealthComponent::FindHealthComponent(Pawn))
	{
		if (UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(this))
		{
			Respawn->ActivateCheckpoint(this);
		}
	}
}
