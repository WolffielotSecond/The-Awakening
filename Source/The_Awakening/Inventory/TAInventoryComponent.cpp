#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TAClothingDefinition.h"
#include "Core/TALocalizeSubsystem.h"

UTAInventoryComponent::UTAInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

int32 UTAInventoryComponent::TryAddItemToClothing(FTAClothingInstance& Clothing, UTAItemDefinition* ItemDef, int32 Count)
{
	if (!ItemDef || Count <= 0)
	{
		return 0;
	}

	int32 Remaining = Count;

	// 1) 堆叠
	if (ItemDef->bCanStack)
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
	while (Remaining > 0)
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

	int32 Remaining = Count;
	Remaining -= TryAddItemToClothing(InnerClothing, ItemDef, Remaining);

	if (Remaining > 0 && OuterClothing.IsValid())
	{
		Remaining -= TryAddItemToClothing(OuterClothing, ItemDef, Remaining);
	}

	const int32 Added = Count - Remaining;
	if (Added > 0)
	{
		ReorganizeInventory();
		RebuildFlattenedSlots();
		NotifyUpdated();
	}
	return Added;
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

	ReorganizeInventory();
	RebuildFlattenedSlots();
	NotifyUpdated();
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

	ReorganizeInventory();
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

	FTAInventorySlot* SourceSlot = nullptr;
	FTAInventorySlot* TargetSlot = nullptr;
	const bool bSourceStorySlot = SourceSlotIndex <= -100 && SourceSlotIndex > -100 - StorySlotCount;
	const bool bTargetStorySlot = TargetSlotIndex <= -100 && TargetSlotIndex > -100 - StorySlotCount;

	if (bSourceStorySlot != bTargetStorySlot)
	{
		return false;
	}

	if (bSourceStorySlot)
	{
		const int32 SourceStoryIndex = -100 - SourceSlotIndex;
		const int32 TargetStoryIndex = -100 - TargetSlotIndex;
		if (!StorySlots.IsValidIndex(SourceStoryIndex) || !StorySlots.IsValidIndex(TargetStoryIndex))
		{
			return false;
		}
		SourceSlot = &StorySlots[SourceStoryIndex];
		TargetSlot = &StorySlots[TargetStoryIndex];
	}
	else
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
