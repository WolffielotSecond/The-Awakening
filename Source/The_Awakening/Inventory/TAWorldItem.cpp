#include "Inventory/TAWorldItem.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TAInventoryComponent.h"
#include "Core/TAPlayerState.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "UI/TAPromptWidgetUtils.h"
#include "Scan/TAScannableComponent.h"

ATAWorldItem::ATAWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->InitSphereRadius(60.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidget->SetupAttachment(RootComponent);
	PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidget->SetDrawAtDesiredSize(true);
	PromptWidget->SetVisibility(false);

	ScannableComponent = CreateDefaultSubobject<UTAScannableComponent>(TEXT("ScannableComponent"));
	ScannableComponent->ScanInfo.TargetType = ETAScanTargetType::Item;
	ScannableComponent->CustomDepthStencilValue = 2;
}

void ATAWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent)
	{
		MeshComponent->SetRenderCustomDepth(true);
		MeshComponent->SetCustomDepthStencilValue(2);
	}
	if (ScannableComponent && ItemDef)
	{
		FTAScanTargetInfo Info = ScannableComponent->GetScanInfo();
		Info.Name = ItemDef->DisplayName;
		Info.Description = ItemDef->Description;
		Info.TargetType = ETAScanTargetType::Item;
		ScannableComponent->SetScanInfo(Info);
	}
}

void ATAWorldItem::SetupItem(UTAItemDefinition* InDef, int32 InCount)
{
	ItemDef = InDef;
	Count = FMath::Max(1, InCount);
	if (ScannableComponent && ItemDef)
	{
		FTAScanTargetInfo Info = ScannableComponent->GetScanInfo();
		Info.Name = ItemDef->DisplayName;
		Info.Description = ItemDef->Description;
		Info.TargetType = ETAScanTargetType::Item;
		ScannableComponent->SetScanInfo(Info);
	}
}

bool ATAWorldItem::IsCurrencyPickup() const
{
	return ItemDef && ItemDef->bIsCurrency;
}

FString ATAWorldItem::GetPromptTextId() const
{
	return PromptTextId.IsEmpty() ? TEXT("Interact_PickUp") : PromptTextId;
}

bool ATAWorldItem::CanInteract_Implementation(AActor* Interactor) const
{
	return ItemDef != nullptr && Count > 0 && Interactor != nullptr;
}

FText ATAWorldItem::GetInteractText_Implementation() const
{
	return FText::FromString(GetPromptTextId());
}

void ATAWorldItem::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor || !ItemDef || Count <= 0)
	{
		return;
	}

	if (IsCurrencyPickup())
	{
		if (TryPickupAsCurrency(Interactor))
		{
			Destroy();
		}
		return;
	}

	UTAInventoryComponent* Inventory = Interactor->FindComponentByClass<UTAInventoryComponent>();
	if (!Inventory)
	{
		return;
	}

	if (TryPickupAsInventory(Interactor, Inventory))
	{
		// 全部捡完才销毁；部分捡起时 Count 已在内部减少
		if (Count <= 0)
		{
			Destroy();
		}
	}
}

bool ATAWorldItem::TryPickupAsCurrency(AActor* Interactor)
{
	APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn)
	{
		return false;
	}

	ATAPlayerState* PS = Pawn->GetPlayerState<ATAPlayerState>();
	if (!PS)
	{
		return false;
	}

	const int32 Amount = ItemDef->Value * Count;
	PS->AddMoney(Amount);
	Count = 0;
	if (UTAInventoryComponent* Inventory = Interactor->FindComponentByClass<UTAInventoryComponent>())
	{
		Inventory->NotifyUpdated();
	}
	return true;
}

bool ATAWorldItem::TryPickupAsInventory(AActor* Interactor, UTAInventoryComponent* Inventory)
{
	if (!Inventory || !ItemDef)
	{
		return false;
	}

	const int32 Added = Inventory->TryAddItem(ItemDef, Count);
	if (Added <= 0)
	{
		return false;
	}

	Count -= Added;
	return true;
}

void ATAWorldItem::SetPromptVisible(bool bVisible)
{
	if (PromptWidget)
	{
		PromptWidget->SetVisibility(bVisible);
		if (bVisible && !PromptWidget->GetUserWidgetObject() && PromptWidget->GetWidgetClass())
		{
			PromptWidget->InitWidget();
		}
	}
}

void ATAWorldItem::RefreshPrompt(UTexture2D* KeyIcon, const FText& PromptText)
{
	if (!PromptWidget)
	{
		return;
	}

	UUserWidget* Widget = PromptWidget->GetUserWidgetObject();
	if (!Widget)
	{
		return;
	}
	FTAPromptWidgetUtils::ApplyPrompt(Widget, KeyIcon, PromptText);
}
