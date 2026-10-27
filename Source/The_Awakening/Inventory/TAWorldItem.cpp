#include "Inventory/TAWorldItem.h"
#include "Inventory/TAItemSettings.h"
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
#include "Engine/StaticMesh.h"
#include "The_AwakeningCharacter.h"
#include "UObject/ConstructorHelpers.h"

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

	// 掉落物品没有头顶提示：交互提示完全由拾取列表 UI（UTANearbyPickupComponent）负责，不加入统一提示系统

	ScannableComponent = CreateDefaultSubobject<UTAScannableComponent>(TEXT("ScannableComponent"));
	ScannableComponent->ScanInfo.TargetType = ETAScanTargetType::Item;
	ScannableComponent->CustomDepthStencilValue = 2;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	FallbackMesh = CubeAsset.Object;
}

void ATAWorldItem::ApplyFallbackMesh()
{
	if (!MeshComponent || MeshComponent->GetStaticMesh())
	{
		return;
	}
	if (ItemDef && ItemDef->PreviewMesh)
	{
		MeshComponent->SetStaticMesh(ItemDef->PreviewMesh);
		MeshComponent->SetRelativeTransform(ItemDef->PreviewTransform);
	}
	else if (FallbackMesh)
	{
		MeshComponent->SetStaticMesh(FallbackMesh);
		MeshComponent->SetRelativeScale3D(FVector(0.25f));
	}
}

void ATAWorldItem::BeginPlay()
{
	Super::BeginPlay();

	ApplyFallbackMesh();
	// 丢失材质（槽位为空或引擎默认材质）换成项目设置里的兜底材质
	UTAItemSettings::ApplyMissingMaterialFallback(MeshComponent);
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

FString ATAWorldItem::GetDisplayNameTextId() const
{
	return ItemDef ? ItemDef->DisplayName.ToString() : FString();
}

UTexture2D* ATAWorldItem::GetDisplayIcon() const
{
	return ItemDef ? UTAItemSettings::ResolveIcon(ItemDef->Icon.Get()) : nullptr;
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
		// 屏幕右侧拾取提示（钱直接加金额，不弹提示）
		if (AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(Interactor))
		{
			Character->NotifyItemPickedUp(ItemDef);
		}
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

	// Value 是小数面值，钱包是整数：按总额四舍五入，避免逐个取整累积误差
	const int32 Amount = FMath::RoundToInt(ItemDef->Value * Count);
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

	TArray<int32> ChangedSlots;
	const int32 Added = Inventory->TryAddItem(ItemDef, Count, ChangedSlots);
	// 迷你背包：放进去的格子闪烁；有物品放不下时提示背包已满。剧情物品不占口袋格，不显示
	if (ItemDef->ItemType != ETAItemType::QuestItem)
	{
		if (AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(Interactor))
		{
			Character->NotifyInventoryPickupResult(ChangedSlots, Added < Count);
		}
	}
	if (Added <= 0)
	{
		return false;
	}

	Count -= Added;
	return true;
}

