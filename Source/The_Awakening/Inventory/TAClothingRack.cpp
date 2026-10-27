#include "Inventory/TAClothingRack.h"
#include "Inventory/TAClothingDefinition.h"
#include "Interaction/TAInteractionBehaviorComponent.h"
#include "Health/TARollbackSubsystem.h"
#include "Scan/TAScannableComponent.h"
#include "UI/Inventory/TAClothingSwapWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATAClothingRack::ATAClothingRack()
{
	InteractTextId = TEXT("Interact_ClothingRack");

	ScannableComponent = CreateDefaultSubobject<UTAScannableComponent>(TEXT("ScannableComponent"));
	ScannableComponent->ScanInfo.TargetType = ETAScanTargetType::Generic;
}

void ATAClothingRack::BeginPlay()
{
	Super::BeginPlay();

	if (ScannableComponent)
	{
		ConfiguredScanInfo = ScannableComponent->GetScanInfo();
	}
	StoredClothing = FTAClothingInstance();
	if (InitialClothing && InitialClothing->Layer == ETAClothingLayer::Outer
		&& UTAInventoryComponent::BuildClothingInstance(InitialClothing, StoredClothing))
	{
		int32 ItemIndex = 0;
		for (FTAPocketRuntime& Pocket : StoredClothing.Pockets)
		{
			for (FTAInventorySlot& Slot : Pocket.Slots)
			{
				if (InitialPocketItems.IsValidIndex(ItemIndex) && !InitialPocketItems[ItemIndex].IsEmpty())
				{
					Slot = InitialPocketItems[ItemIndex];
				}
				++ItemIndex;
			}
		}
	}
	else if (InitialClothing)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClothingRack %s: InitialClothing '%s' is not an outer layer; rack starts empty."),
			*GetName(), *InitialClothing->GetName());
	}
	ApplyMesh();
	ApplyScanInfo();
}

bool ATAClothingRack::CanInteract_Implementation(AActor* Interactor) const
{
	return Super::CanInteract_Implementation(Interactor) && Interactor->FindComponentByClass<UTAInventoryComponent>() != nullptr;
}

void ATAClothingRack::OnInteract_Implementation(AActor* Interactor)
{
	// 条件不满足时走交互行为组件的拒绝流程（与普通交互物一致）
	if (InteractionBehavior && !InteractionBehavior->MeetsConditions(Interactor))
	{
		InteractionBehavior->TryExecute(Interactor);
		return;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UTAInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UTAInventoryComponent>() : nullptr;
	if (!PC || !PC->IsLocalController() || !Inventory)
	{
		return;
	}
	UTAClothingSwapWidget::Open(PC, this, Inventory, SwapWidgetClass);
}

void ATAClothingRack::NotifyClothingChanged()
{
	ApplyMesh();
	ApplyScanInfo();
	OnRackChanged.Broadcast();
}

void ATAClothingRack::RecordUndo()
{
	UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this);
	if (!Rollback || !Rollback->IsRecording())
	{
		return;
	}
	Rollback->RecordUndo(TEXT("ClothingRack"),
		[WeakThis = TWeakObjectPtr<ATAClothingRack>(this), Previous = StoredClothing]
		{
			if (ATAClothingRack* Self = WeakThis.Get())
			{
				Self->StoredClothing = Previous;
				Self->NotifyClothingChanged();
			}
		});
}

void ATAClothingRack::ApplyMesh()
{
	UStaticMesh* Mesh = HasClothing() ? CoatMesh.Get() : EmptyMesh.Get();
	if (MeshComponent && Mesh)
	{
		MeshComponent->SetStaticMesh(Mesh);
	}
}

void ATAClothingRack::ApplyScanInfo()
{
	if (!ScannableComponent)
	{
		return;
	}
	FTAScanTargetInfo Info = ConfiguredScanInfo;
	if (const UTAClothingDefinition* Def = StoredClothing.Definition)
	{
		// 与物品一致：Name / Description 填本地化 ID，由扫描界面翻译
		Info.Name = FText::FromString(Def->DisplayNameId);
		Info.Description = FText::FromString(Def->DescriptionId);
	}
	ScannableComponent->SetScanInfo(Info);
}
