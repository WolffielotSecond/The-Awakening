#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TAClothingDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "Health/TARollbackSubsystem.h"
#include "Inventory/TAWorldItem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogTAInventory, Log, All);

UTAInventoryComponent::UTAInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	StorySlots.SetNum(StorySlotCount);
}

void UTAInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	StorySlots.SetNum(StorySlotCount);
	InitDefaultClothing();
	RebuildFlattenedSlots();
	NotifyUpdated();
}

void UTAInventoryComponent::NotifyUpdated()
{
	OnInventoryUpdated.Broadcast();
}

bool UTAInventoryComponent::BuildClothingInstance(UTAClothingDefinition* Def, FTAClothingInstance& OutInstance) const
{
	OutInstance = FTAClothingInstance();
	if (!Def)
	{
		return false;
	}

	OutInstance.Definition = Def;
	OutInstance.Pockets.Reset();

	for (const FTAPocketDef& PocketDef : Def->Pockets)
	{
		FTAPocketRuntime Pocket;
		Pocket.PocketId = PocketDef.PocketId.IsNone() ? TEXT("Main") : PocketDef.PocketId;
		Pocket.Slots.SetNum(FMath::Max(0, PocketDef.SlotCount));
		OutInstance.Pockets.Add(MoveTemp(Pocket));
	}

	// 没有配置口袋时给 0 格，避免空 Definition 被当成有效装备乱加东西
	return OutInstance.Pockets.Num() > 0 || Def->GetTotalSlotCount() == 0;
}

void UTAInventoryComponent::InitDefaultClothing()
{
	if (DefaultInnerClothing)
	{
		BuildClothingInstance(DefaultInnerClothing, InnerClothing);
	}
	else
	{
		// 兜底：无 DA 时也有 2 格，避免背包全坏
		InnerClothing = FTAClothingInstance();
		FTAPocketRuntime Pocket;
		Pocket.PocketId = TEXT("Main");
		Pocket.Slots.SetNum(2);
		InnerClothing.Pockets.Add(Pocket);
	}

	if (DefaultOuterClothing && DefaultOuterClothing->Layer == ETAClothingLayer::Outer)
	{
		BuildClothingInstance(DefaultOuterClothing, OuterClothing);
	}
	else
	{
		OuterClothing = FTAClothingInstance();
	}
}

void UTAInventoryComponent::RebuildFlattenedSlots()
{
	FlattenedSlots.Reset();

	auto AppendClothing = [this](const FTAClothingInstance& Clothing)
		{
			for (const FTAPocketRuntime& Pocket : Clothing.Pockets)
			{
				FlattenedSlots.Append(Pocket.Slots);
			}
		};

	AppendClothing(InnerClothing);
	if (OuterClothing.IsValid())
	{
		AppendClothing(OuterClothing);
	}
}

bool UTAInventoryComponent::ResolveFlatIndex(int32 FlatIndex, FTAClothingInstance*& OutClothing, int32& OutPocketIndex, int32& OutSlotIndex)
{
	OutClothing = nullptr;
	OutPocketIndex = INDEX_NONE;
	OutSlotIndex = INDEX_NONE;

	if (FlatIndex < 0)
	{
		return false;
	}

	int32 Remaining = FlatIndex;

	auto Walk = [&](FTAClothingInstance& Clothing) -> bool
		{
			for (int32 p = 0; p < Clothing.Pockets.Num(); ++p)
			{
				const int32 Num = Clothing.Pockets[p].Slots.Num();
				if (Remaining < Num)
				{
					OutClothing = &Clothing;
					OutPocketIndex = p;
					OutSlotIndex = Remaining;
					return true;
				}
				Remaining -= Num;
			}
			return false;
		};

	if (Walk(InnerClothing))
	{
		return true;
	}
	if (OuterClothing.IsValid() && Walk(OuterClothing))
	{
		return true;
	}
	return false;
}

int32 UTAInventoryComponent::TryAddItemToClothing(FTAClothingInstance& Clothing, UTAItemDefinition* ItemDef, int32 Count, bool bFillStacks, bool bFillEmpty)
{
	if (!ItemDef || Count <= 0)
	{
		return 0;
	}

	int32 Remaining = Count;

	// 1) 堆叠
	if (ItemDef->bCanStack && bFillStacks)
	{
		for (FTAPocketRuntime& Pocket : Clothing.Pockets)
		{
			for (FTAInventorySlot& Slot : Pocket.Slots)
			{
				if (Slot.ItemDef == ItemDef && Slot.Count < ItemDef->MaxStack)
				{
					const int32 CanAdd = FMath::Min(Remaining, ItemDef->MaxStack - Slot.Count);
					Slot.Count += CanAdd;
					Remaining -= CanAdd;
					if (Remaining <= 0)
					{
						return Count;
					}
				}
			}
		}
	}

	// 2) 空格
	while (bFillEmpty && Remaining > 0)
	{
		FTAInventorySlot* EmptySlot = nullptr;
		for (FTAPocketRuntime& Pocket : Clothing.Pockets)
		{
			for (FTAInventorySlot& Slot : Pocket.Slots)
			{
				if (Slot.IsEmpty())
				{
					EmptySlot = &Slot;
					break;
				}
			}
			if (EmptySlot)
			{
				break;
			}
		}

		if (!EmptySlot)
		{
			break;
		}

		EmptySlot->ItemDef = ItemDef;
		if (ItemDef->bCanStack)
		{
			const int32 CanAdd = FMath::Min(Remaining, ItemDef->MaxStack);
			EmptySlot->Count = CanAdd;
			EmptySlot->Durability = 0.f;
			Remaining -= CanAdd;
		}
		else
		{
			EmptySlot->Count = 1;
			EmptySlot->Durability = (ItemDef->ItemType == ETAItemType::Weapon) ? ItemDef->MaxDurability : 0.f;
			Remaining -= 1;
		}
	}

	return Count - Remaining;
}

int32 UTAInventoryComponent::TryAddItem(UTAItemDefinition* ItemDef, int32 Count)
{
	if (!ItemDef || Count <= 0)
	{
		return 0;
	}
	if (ItemDef->ItemType == ETAItemType::QuestItem)
	{
		return TryAddStoryItem(ItemDef);
	}

	// 先补满所有口袋里已有的同类堆叠，再按顺序放进第一个空格。
	// 不再自动整理：已有物品的位置（玩家拖拽摆放、拆开的堆叠）保持不变
	int32 Remaining = Count;
	Remaining -= TryAddItemToClothing(InnerClothing, ItemDef, Remaining, true, false);
	if (Remaining > 0 && OuterClothing.IsValid())
	{
		Remaining -= TryAddItemToClothing(OuterClothing, ItemDef, Remaining, true, false);
	}
	if (Remaining > 0)
	{
		Remaining -= TryAddItemToClothing(InnerClothing, ItemDef, Remaining, false, true);
	}
	if (Remaining > 0 && OuterClothing.IsValid())
	{
		Remaining -= TryAddItemToClothing(OuterClothing, ItemDef, Remaining, false, true);
	}

	const int32 Added = Count - Remaining;
	if (Added > 0)
	{
		RebuildFlattenedSlots();
		NotifyUpdated();
	}
	return Added;
}

int32 UTAInventoryComponent::TryAddStoryItem(UTAItemDefinition* ItemDef)
{
	if (!ItemDef || ItemDef->ItemType != ETAItemType::QuestItem)
	{
		return 0;
	}
	if (!StorySlots.IsValidIndex(ItemDef->StorySlotIndex))
	{
		UE_LOG(LogTAInventory, Warning, TEXT("Story item '%s' is not bound to a story slot (StorySlotIndex=%d)."),
			*ItemDef->GetName(), ItemDef->StorySlotIndex);
		return 0;
	}

	FTAInventorySlot& Slot = StorySlots[ItemDef->StorySlotIndex];
	if (HasStoryItem(ItemDef))
	{
		return 0;
	}
	if (!Slot.IsEmpty() && Slot.ItemDef != ItemDef->UpgradesFrom)
	{
		UE_LOG(LogTAInventory, Warning, TEXT("Story slot %d is occupied by '%s'; '%s' rejected."),
			ItemDef->StorySlotIndex, *Slot.ItemDef->GetName(), *ItemDef->GetName());
		return 0;
	}

	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this); Rollback && Rollback->IsRecording())
	{
		Rollback->RecordUndo(TEXT("Inventory.StoryItem"),
			[WeakThis = TWeakObjectPtr<UTAInventoryComponent>(this), SlotIndex = ItemDef->StorySlotIndex, Previous = Slot]
			{
				if (UTAInventoryComponent* Self = WeakThis.Get())
				{
					Self->StorySlots[SlotIndex] = Previous;
					Self->NotifyUpdated();
				}
			});
	}

	// 升级版直接替换原位
	Slot = FTAInventorySlot();
	Slot.ItemDef = ItemDef;
	Slot.Count = 1;
	NotifyUpdated();
	return 1;
}

bool UTAInventoryComponent::HasStoryItem(const UTAItemDefinition* ItemDef) const
{
	if (!ItemDef)
	{
		return false;
	}
	for (const FTAInventorySlot& Slot : StorySlots)
	{
		// 沿升级链向前找：持有升级版也算持有原版。限制步数，防止配置成环
		const UTAItemDefinition* Def = Slot.IsEmpty() ? nullptr : Slot.ItemDef.Get();
		for (int32 Step = 0; Def && Step < 16; ++Step, Def = Def->UpgradesFrom)
		{
			if (Def == ItemDef)
			{
				return true;
			}
		}
	}
	return false;
}

int32 UTAInventoryComponent::RemoveFromSlot(int32 SlotIndex, int32 Count)
{
	FTAClothingInstance* Clothing = nullptr;
	int32 PocketIndex = INDEX_NONE;
	int32 SlotInPocket = INDEX_NONE;
	if (!ResolveFlatIndex(SlotIndex, Clothing, PocketIndex, SlotInPocket) || Count <= 0)
	{
		return 0;
	}

	FTAInventorySlot& Slot = Clothing->Pockets[PocketIndex].Slots[SlotInPocket];
	if (Slot.IsEmpty())
	{
		return 0;
	}

	const int32 Removed = FMath::Min(Count, Slot.Count);
	Slot.Count -= Removed;
	if (Slot.Count <= 0)
	{
		Slot = FTAInventorySlot();
	}

	RebuildFlattenedSlots();
	NotifyUpdated();
	return Removed;
}

int32 UTAInventoryComponent::RemoveItem(UTAItemDefinition* ItemDef, int32 Count)
{
	int32 Removed = 0;
	while (ItemDef && Removed < Count)
	{
		// RemoveFromSlot 会重排格子，每次都重新查找
		const int32 SlotIndex = FlattenedSlots.IndexOfByPredicate([ItemDef](const FTAInventorySlot& Slot) { return Slot.ItemDef == ItemDef; });
		const int32 Step = SlotIndex == INDEX_NONE ? 0 : RemoveFromSlot(SlotIndex, Count - Removed);
		if (Step <= 0)
		{
			break;
		}
		Removed += Step;
	}
	return Removed;
}

int32 UTAInventoryComponent::GetItemCount(UTAItemDefinition* ItemDef) const
{
	if (!ItemDef)
	{
		return 0;
	}

	int32 Total = 0;
	auto CountIn = [&](const FTAClothingInstance& Clothing)
		{
			for (const FTAPocketRuntime& Pocket : Clothing.Pockets)
			{
				for (const FTAInventorySlot& Slot : Pocket.Slots)
				{
					if (Slot.ItemDef == ItemDef)
					{
						Total += Slot.Count;
					}
				}
			}
		};

	CountIn(InnerClothing);
	if (OuterClothing.IsValid())
	{
		CountIn(OuterClothing);
	}
	return Total;
}

bool UTAInventoryComponent::IsSlotEmpty(int32 SlotIndex) const
{
	if (!FlattenedSlots.IsValidIndex(SlotIndex))
	{
		return true;
	}
	return FlattenedSlots[SlotIndex].IsEmpty();
}

bool UTAInventoryComponent::HasEmptySlot() const
{
	for (const FTAInventorySlot& Slot : FlattenedSlots)
	{
		if (Slot.IsEmpty())
		{
			return true;
		}
	}
	return false;
}

bool UTAInventoryComponent::DropFromSlot(int32 SlotIndex, int32 Count, FTAInventorySlot& OutDropped)
{
	OutDropped = FTAInventorySlot();

	FTAClothingInstance* Clothing = nullptr;
	int32 PocketIndex = INDEX_NONE;
	int32 SlotInPocket = INDEX_NONE;
	if (!ResolveFlatIndex(SlotIndex, Clothing, PocketIndex, SlotInPocket) || Count <= 0)
	{
		return false;
	}

	FTAInventorySlot& Slot = Clothing->Pockets[PocketIndex].Slots[SlotInPocket];
	if (Slot.IsEmpty())
	{
		return false;
	}

	const int32 DropCount = FMath::Min(Count, Slot.Count);
	OutDropped.ItemDef = Slot.ItemDef;
	OutDropped.Count = DropCount;
	OutDropped.Durability = Slot.Durability;

	Slot.Count -= DropCount;
	if (Slot.Count <= 0)
	{
		Slot = FTAInventorySlot();
	}

	RebuildFlattenedSlots();
	NotifyUpdated();
	return true;
}

bool UTAInventoryComponent::SplitStack(int32 SlotIndex, int32 Count)
{
	FTAClothingInstance* SourceClothing = nullptr;
	int32 SourcePocket = INDEX_NONE;
	int32 SourceIndex = INDEX_NONE;
	if (IsStorySlotUIIndex(SlotIndex) || !ResolveFlatIndex(SlotIndex, SourceClothing, SourcePocket, SourceIndex))
	{
		return false;
	}
	FTAInventorySlot& Source = SourceClothing->Pockets[SourcePocket].Slots[SourceIndex];
	if (Source.IsEmpty() || Count <= 0 || Count > Source.Count)
	{
		return false;
	}
	// 第一个空格（按界面顺序：内衬口袋在前，外套口袋在后）
	const int32 EmptyIndex = FlattenedSlots.IndexOfByPredicate([](const FTAInventorySlot& Slot) { return Slot.IsEmpty(); });
	FTAClothingInstance* TargetClothing = nullptr;
	int32 TargetPocket = INDEX_NONE;
	int32 TargetIndex = INDEX_NONE;
	if (EmptyIndex == INDEX_NONE || !ResolveFlatIndex(EmptyIndex, TargetClothing, TargetPocket, TargetIndex))
	{
		return false;
	}
	FTAInventorySlot& Target = TargetClothing->Pockets[TargetPocket].Slots[TargetIndex];
	Target.ItemDef = Source.ItemDef;
	Target.Count = Count;
	Target.Durability = Source.Durability;
	Source.Count -= Count;
	if (Source.Count <= 0)
	{
		Source = FTAInventorySlot();
	}
	RebuildFlattenedSlots();
	NotifyUpdated();
	return true;
}

bool UTAInventoryComponent::MoveItemBetweenSlots(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
	if (SourceSlotIndex == TargetSlotIndex)
	{
		return false;
	}

	// 剧情格固定绑定物品：不能拖出、拖入或互换
	if (IsStorySlotUIIndex(SourceSlotIndex) || IsStorySlotUIIndex(TargetSlotIndex))
	{
		return false;
	}

	FTAInventorySlot* SourceSlot = nullptr;
	FTAInventorySlot* TargetSlot = nullptr;
	{
		FTAClothingInstance* SourceClothing = nullptr;
		FTAClothingInstance* TargetClothing = nullptr;
		int32 SourcePocketIndex = INDEX_NONE;
		int32 TargetPocketIndex = INDEX_NONE;
		int32 SourceIndexInPocket = INDEX_NONE;
		int32 TargetIndexInPocket = INDEX_NONE;
		if (!ResolveFlatIndex(SourceSlotIndex, SourceClothing, SourcePocketIndex, SourceIndexInPocket)
			|| !ResolveFlatIndex(TargetSlotIndex, TargetClothing, TargetPocketIndex, TargetIndexInPocket))
		{
			return false;
		}
		SourceSlot = &SourceClothing->Pockets[SourcePocketIndex].Slots[SourceIndexInPocket];
		TargetSlot = &TargetClothing->Pockets[TargetPocketIndex].Slots[TargetIndexInPocket];
	}

	if (!SourceSlot || !TargetSlot || SourceSlot->IsEmpty())
	{
		return false;
	}

	if (TargetSlot->IsEmpty())
	{
		Swap(*SourceSlot, *TargetSlot);
	}
	else if (SourceSlot->ItemDef == TargetSlot->ItemDef && SourceSlot->ItemDef->bCanStack)
	{
		const int32 MaxStack = FMath::Max(1, SourceSlot->ItemDef->MaxStack);
		const int32 TransferCount = FMath::Min(SourceSlot->Count, FMath::Max(0, MaxStack - TargetSlot->Count));
		if (TransferCount <= 0)
		{
			return false;
		}

		TargetSlot->Count += TransferCount;
		SourceSlot->Count -= TransferCount;
		if (SourceSlot->Count <= 0)
		{
			*SourceSlot = FTAInventorySlot();
		}
	}
	else
	{
		Swap(*SourceSlot, *TargetSlot);
	}

	RebuildFlattenedSlots();
	NotifyUpdated();
	return true;
}

bool UTAInventoryComponent::EquipOuter(UTAClothingDefinition* OuterDef)
{
	if (!OuterDef || OuterDef->Layer != ETAClothingLayer::Outer)
	{
		return false;
	}
	if (OuterClothing.IsValid())
	{
		// 已有外套：先卸再装（由 UI/玩法决定）；这里直接拒绝
		return false;
	}

	if (!BuildClothingInstance(OuterDef, OuterClothing))
	{
		return false;
	}

	RebuildFlattenedSlots();
	NotifyUpdated();
	return true;
}

bool UTAInventoryComponent::UnequipOuter(FTAClothingInstance& OutInstance)
{
	OutInstance = FTAClothingInstance();
	if (!OuterClothing.IsValid())
	{
		return false;
	}

	OutInstance = OuterClothing;
	OuterClothing = FTAClothingInstance();
	RebuildFlattenedSlots();
	NotifyUpdated();
	return true;
}

FString UTAInventoryComponent::GetSortName(UTAItemDefinition* ItemDef) const
{
	if (!ItemDef)
	{
		return FString();
	}

	const FString TextId = ItemDef->DisplayName.ToString();
	if (TextId.IsEmpty())
	{
		return FString();
	}

	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				return Loc->GetText(TextId).ToString();
			}
		}
	}
	return TextId;
}

void UTAInventoryComponent::ReorganizeClothing(FTAClothingInstance& Clothing)
{
	// 收集该衣服所有非空格
	TArray<FTAInventorySlot> Packed;
	int32 TotalSlots = 0;
	for (const FTAPocketRuntime& Pocket : Clothing.Pockets)
	{
		TotalSlots += Pocket.Slots.Num();
		for (const FTAInventorySlot& Slot : Pocket.Slots)
		{
			if (!Slot.IsEmpty())
			{
				Packed.Add(Slot);
			}
		}
	}

	if (TotalSlots <= 0)
	{
		return;
	}

	// 合并堆叠
	TMap<UTAItemDefinition*, int32> StackCounts;
	TArray<FTAInventorySlot> NonStackable;

	for (const FTAInventorySlot& Slot : Packed)
	{
		if (!Slot.ItemDef)
		{
			continue;
		}
		if (Slot.ItemDef->bCanStack)
		{
			StackCounts.FindOrAdd(Slot.ItemDef) += Slot.Count;
		}
		else
		{
			NonStackable.Add(Slot);
		}
	}

	TArray<FTAInventorySlot> Result;
	for (const auto& Pair : StackCounts)
	{
		UTAItemDefinition* Def = Pair.Key;
		int32 Remaining = Pair.Value;
		const int32 MaxStack = FMath::Max(1, Def->MaxStack);
		while (Remaining > 0)
		{
			FTAInventorySlot NewSlot;
			NewSlot.ItemDef = Def;
			NewSlot.Count = FMath::Min(Remaining, MaxStack);
			Result.Add(NewSlot);
			Remaining -= NewSlot.Count;
		}
	}
	Result.Append(NonStackable);

	Result.Sort([this](const FTAInventorySlot& A, const FTAInventorySlot& B)
		{
			const UTAItemDefinition* DefA = A.ItemDef;
			const UTAItemDefinition* DefB = B.ItemDef;
			if (!DefA || !DefB)
			{
				return DefA != nullptr;
			}
			if (DefA->ItemType != DefB->ItemType)
			{
				return static_cast<uint8>(DefA->ItemType) < static_cast<uint8>(DefB->ItemType);
			}
			const int32 NameCmp = GetSortName(A.ItemDef).Compare(GetSortName(B.ItemDef), ESearchCase::IgnoreCase);
			if (NameCmp != 0)
			{
				return NameCmp < 0;
			}
			return A.Count > B.Count;
		});

	// 写回口袋（按口袋顺序填满）
	int32 WriteIndex = 0;
	for (FTAPocketRuntime& Pocket : Clothing.Pockets)
	{
		for (FTAInventorySlot& Slot : Pocket.Slots)
		{
			if (WriteIndex < Result.Num())
			{
				Slot = Result[WriteIndex++];
			}
			else
			{
				Slot = FTAInventorySlot();
			}
		}
	}
}

void UTAInventoryComponent::ReorganizeInventory()
{
	ReorganizeClothing(InnerClothing);
	if (OuterClothing.IsValid())
	{
		ReorganizeClothing(OuterClothing);
	}
}

// ==================== 丢弃 ====================

bool UTAInventoryComponent::FindDropLocation(const AActor* Dropper, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World || !Dropper)
	{
		return false;
	}
	const ACharacter* Character = Cast<ACharacter>(Dropper);
	const float HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Center = Dropper->GetActorLocation();
	const FVector Feet = Center - FVector(0.f, 0.f, HalfHeight);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TAInventoryDrop), false, Dropper);
	auto IsOccupied = [&](const FVector& Point)
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, Point, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(DropClearRadius), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Cast<ATAWorldItem>(Overlap.GetActor()))
			{
				return true;
			}
		}
		return false;
	};
	// 地面点：从候选点上方往下找地面；找不到（悬崖）、高低差过大、坡度过陡都不行
	auto FindFloor = [&](const FVector& Candidate, FVector& OutFloor)
	{
		FHitResult Hit;
		const FVector Start = Candidate + FVector(0.f, 0.f, HalfHeight);
		const FVector End = Candidate - FVector(0.f, 0.f, DropMaxStepHeight + 1.f);
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || Hit.bStartPenetrating)
		{
			return false;
		}
		const float Rise = FMath::Abs(Hit.ImpactPoint.Z - Feet.Z);
		if (Rise > DropMaxStepHeight)
		{
			return false;
		}
		const float Horizontal = FVector::Dist2D(Hit.ImpactPoint, Feet);
		if (Horizontal > KINDA_SMALL_NUMBER && FMath::RadiansToDegrees(FMath::Atan2(Rise, Horizontal)) > DropMaxSlopeAngle)
		{
			return false;
		}
		if (FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z, -1.f, 1.f))) > DropMaxSlopeAngle)
		{
			return false;
		}
		OutFloor = Hit.ImpactPoint;
		return true;
	};

	// 1. 脚下
	FVector Floor;
	if (FindFloor(Feet, Floor) && !IsOccupied(Floor))
	{
		OutLocation = Floor;
		return true;
	}
	// 2. 一圈 6 个方向，从正前方开始每 60°
	const FVector Forward = Dropper->GetActorForwardVector().GetSafeNormal2D();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FVector Direction = Forward.RotateAngleAxis(60.f * Index, FVector::UpVector);
		const FVector Candidate = Feet + Direction * DropDistance;
		// 被墙挡住：从角色中心水平看向候选点上方
		FHitResult Wall;
		if (World->LineTraceSingleByChannel(Wall, Center, FVector(Candidate.X, Candidate.Y, Center.Z), ECC_Visibility, Params))
		{
			continue;
		}
		if (FindFloor(Candidate, Floor) && !IsOccupied(Floor))
		{
			OutLocation = Floor;
			return true;
		}
	}
	return false;
}

bool UTAInventoryComponent::DropSlotToWorld(int32 SlotIndex, int32 Count, AActor* Dropper)
{
	if (IsStorySlotUIIndex(SlotIndex) || !FlattenedSlots.IsValidIndex(SlotIndex) || FlattenedSlots[SlotIndex].IsEmpty() || Count <= 0)
	{
		return false;
	}
	UTAItemDefinition* ItemDef = FlattenedSlots[SlotIndex].ItemDef;
	if (!ItemDef || ItemDef->ItemType == ETAItemType::QuestItem)
	{
		return false;
	}
	FVector Location;
	if (!FindDropLocation(Dropper, Location))
	{
		return false;
	}
	UClass* WorldItemClass = ItemDef->WorldItemClass.LoadSynchronous();
	if (!WorldItemClass)
	{
		WorldItemClass = ATAWorldItem::StaticClass();
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform SpawnTransform(FRotator(0.f, Dropper ? Dropper->GetActorRotation().Yaw : 0.f, 0.f), Location + FVector(0.f, 0.f, DropSpawnHeight));
	ATAWorldItem* WorldItem = GetWorld()->SpawnActorDeferred<ATAWorldItem>(WorldItemClass, SpawnTransform, nullptr, nullptr, SpawnParams.SpawnCollisionHandlingOverride);
	if (!WorldItem)
	{
		return false;
	}
	FTAInventorySlot Dropped;
	if (!DropFromSlot(SlotIndex, Count, Dropped))
	{
		WorldItem->Destroy();
		return false;
	}
	WorldItem->SetupItem(Dropped.ItemDef, Dropped.Count);
	WorldItem->FinishSpawning(SpawnTransform);
	return true;
}

// ==================== 存档 ====================

namespace
{
	FTASavedSlot SaveSlot(const FTAInventorySlot& Slot)
	{
		FTASavedSlot Saved;
		if (!Slot.IsEmpty())
		{
			Saved.Item = FSoftObjectPath(Slot.ItemDef.Get());
			Saved.Count = Slot.Count;
			Saved.Durability = Slot.Durability;
		}
		return Saved;
	}

	FTAInventorySlot LoadSlot(const FTASavedSlot& Saved)
	{
		FTAInventorySlot Slot;
		if (Saved.Item.IsNull() || Saved.Count <= 0)
		{
			return Slot;
		}
		UTAItemDefinition* Item = Cast<UTAItemDefinition>(Saved.Item.TryLoad());
		if (!Item)
		{
			UE_LOG(LogTAInventory, Warning, TEXT("Save: item '%s' not found; dropped."), *Saved.Item.ToString());
			return Slot;
		}
		Slot.ItemDef = Item;
		Slot.Count = Saved.Count;
		Slot.Durability = Saved.Durability;
		return Slot;
	}

	FTASavedClothing SaveClothing(const FTAClothingInstance& Clothing)
	{
		FTASavedClothing Saved;
		if (!Clothing.IsValid())
		{
			return Saved;
		}
		Saved.Definition = FSoftObjectPath(Clothing.Definition.Get());
		for (const FTAPocketRuntime& Pocket : Clothing.Pockets)
		{
			FTASavedPocket& SavedPocket = Saved.Pockets.AddDefaulted_GetRef();
			for (const FTAInventorySlot& Slot : Pocket.Slots)
			{
				SavedPocket.Slots.Add(SaveSlot(Slot));
			}
		}
		return Saved;
	}
}

FTAInventorySaveData UTAInventoryComponent::GetSaveData() const
{
	FTAInventorySaveData Data;
	Data.Inner = SaveClothing(InnerClothing);
	Data.Outer = SaveClothing(OuterClothing);
	for (const FTAInventorySlot& Slot : StorySlots)
	{
		Data.StorySlots.Add(SaveSlot(Slot));
	}
	return Data;
}

void UTAInventoryComponent::RestoreSaveData(const FTAInventorySaveData& Data)
{
	// 按存档里的衣服定义重建口袋，再按格子位置放回物品
	auto Restore = [this](const FTASavedClothing& Saved, FTAClothingInstance& Clothing, UTAClothingDefinition* Fallback)
	{
		UTAClothingDefinition* Definition = Saved.Definition.IsNull() ? nullptr : Cast<UTAClothingDefinition>(Saved.Definition.TryLoad());
		if (!Definition)
		{
			Definition = Fallback;
		}
		if (!Definition || !BuildClothingInstance(Definition, Clothing))
		{
			Clothing = FTAClothingInstance();
			return;
		}
		for (int32 PocketIndex = 0; PocketIndex < Clothing.Pockets.Num() && PocketIndex < Saved.Pockets.Num(); ++PocketIndex)
		{
			TArray<FTAInventorySlot>& Slots = Clothing.Pockets[PocketIndex].Slots;
			const TArray<FTASavedSlot>& SavedSlots = Saved.Pockets[PocketIndex].Slots;
			for (int32 SlotIndex = 0; SlotIndex < Slots.Num() && SlotIndex < SavedSlots.Num(); ++SlotIndex)
			{
				Slots[SlotIndex] = LoadSlot(SavedSlots[SlotIndex]);
			}
		}
	};
	Restore(Data.Inner, InnerClothing, DefaultInnerClothing);
	Restore(Data.Outer, OuterClothing, nullptr);

	StorySlots.Reset();
	StorySlots.SetNum(StorySlotCount);
	for (int32 Index = 0; Index < StorySlotCount && Index < Data.StorySlots.Num(); ++Index)
	{
		StorySlots[Index] = LoadSlot(Data.StorySlots[Index]);
	}
	RebuildFlattenedSlots();
	NotifyUpdated();
}
