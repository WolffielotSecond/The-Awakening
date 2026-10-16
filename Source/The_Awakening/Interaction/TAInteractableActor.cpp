#include "Interaction/TAInteractableActor.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Components/StaticMeshComponent.h"

ATAInteractableActor::ATAInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// 保留原组件名，已有蓝图 / 关卡实例对它的设置（例如 Widget Class）继续生效
	InteractPromptComponent = CreateDefaultSubobject<UTAInteractionPromptAnchor>(TEXT("InteractPromptComponent"));
	InteractPromptComponent->SetupAttachment(RootComponent);
	// 原先显示时被提示组件改写为 Z=100，这里直接用这个实际显示位置
	InteractPromptComponent->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	InteractPromptComponent->PromptType = ETAPromptType::Interact;
}

void ATAInteractableActor::BeginPlay()
{
	Super::BeginPlay();
}

void ATAInteractableActor::OnInteract_Implementation(AActor* Interactor)
{
	if (Interactor)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s 与 %s 发生了交互"), *Interactor->GetName(), *GetName());
	}
}

bool ATAInteractableActor::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

FText ATAInteractableActor::GetInteractText_Implementation() const
{
	// 保留接口
	if (!InteractText.IsEmpty())
	{
		return InteractText;
	}
	return FText::FromString(InteractTextId);
}

FString ATAInteractableActor::GetPromptTextId() const
{
	if (!InteractTextId.IsEmpty())
	{
		return InteractTextId;
	}
	return InteractText.ToString();
}
