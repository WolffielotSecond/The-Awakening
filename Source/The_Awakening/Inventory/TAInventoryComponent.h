#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/TAItemTypes.h"
#include "Inventory/TAClothingTypes.h"
#include "TAInventoryComponent.generated.h"

class UTAItemDefinition;
class UTAClothingDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

/** 运行时单个口袋 */
USTRUCT(BlueprintType)
struct FTAPocketRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName PocketId = TEXT("Main");

	UPROPERTY(BlueprintReadOnly)
	TArray<FTAInventorySlot> Slots;
};

/** 运行时一件衣服 */
USTRUCT(BlueprintType)
struct FTAClothingInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTAClothingDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TArray<FTAPocketRuntime> Pockets;

	bool IsValid() const { return Definition != nullptr; }
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTAInventoryComponent();

	virtual void BeginPlay() override;

	/** 默认内衬（必填，例如 2 格） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Clothing")
	TObjectPtr<UTAClothingDefinition> DefaultInnerClothing;

	/** 可选：开局就穿的外套 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Clothing")
	TObjectPtr<UTAClothingDefinition> DefaultOuterClothing;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;


	/** 内衬+外套所有口袋格子拼在一起（只读缓存，变更后 Rebuild） */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FTAInventorySlot>& GetSlots() const { return FlattenedSlots; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetMaxSlots() const { return FlattenedSlots.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const FTAClothingInstance& GetInnerClothing() const { return InnerClothing; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const FTAClothingInstance& GetOuterClothing() const { return OuterClothing; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasOuterClothing() const { return OuterClothing.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FTAInventorySlot>& GetStorySlots() const { return StorySlots; }

	static constexpr int32 StorySlotCount = 4;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 TryAddItem(UTAItemDefinition* ItemDef, int32 Count = 1);

	/** 扁平下标：仅针对 GetSlots()，不是剧情格 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveFromSlot(int32 SlotIndex, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemCount(UTAItemDefinition* ItemDef) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsSlotEmpty(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasEmptySlot() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool DropFromSlot(int32 SlotIndex, int32 Count, FTAInventorySlot& OutDropped);

	/** Move, swap, or merge a complete inventory stack between two UI slot indices. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItemBetweenSlots(int32 SourceSlotIndex, int32 TargetSlotIndex);

	/** 装备外套（Layer 必须是 Outer） */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Clothing")
	bool EquipOuter(UTAClothingDefinition* OuterDef);

	/** 卸下外套；OutInstance 带出口袋里的物品，由外部生成世界物 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Clothing")
	bool UnequipOuter(FTAClothingInstance& OutInstance);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void NotifyUpdated();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SortInventory()
	{
		ReorganizeInventory();
		RebuildFlattenedSlots();
		NotifyUpdated();
	}

protected:
	void InitDefaultClothing();
	bool BuildClothingInstance(UTAClothingDefinition* Def, FTAClothingInstance& OutInstance) const;

	/** 在指定衣服实例上尝试加入，返回实际加入数 */
	int32 TryAddItemToClothing(FTAClothingInstance& Clothing, UTAItemDefinition* ItemDef, int32 Count);

	void ReorganizeInventory();
	void ReorganizeClothing(FTAClothingInstance& Clothing);
	void RebuildFlattenedSlots();

	FString GetSortName(UTAItemDefinition* ItemDef) const;

	/** 扁平下标 → 改到具体口袋格子 */
	bool ResolveFlatIndex(int32 FlatIndex, FTAClothingInstance*& OutClothing, int32& OutPocketIndex, int32& OutSlotIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Clothing")
	FTAClothingInstance InnerClothing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Clothing")
	FTAClothingInstance OuterClothing;

	/** 4 个剧情格：不可经 TryAddItem / DropFromSlot 普通流程丢掉 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Story")
	TArray<FTAInventorySlot> StorySlots;

	/** 缓存：内衬口袋 + 外套口袋 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FTAInventorySlot> FlattenedSlots;
};
