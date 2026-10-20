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

/** 存档里的一格：物品资产路径、数量、耐久（空格子 Item 为空） */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASavedSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSoftObjectPath Item;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 Count = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	float Durability = 0.f;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASavedPocket
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TArray<FTASavedSlot> Slots;
};

/** 存档里的一件衣服：定义路径 + 自己的口袋（按格子位置保存） */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASavedClothing
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSoftObjectPath Definition;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TArray<FTASavedPocket> Pockets;
};

/** 背包存档数据（钱在 PlayerState 上，由存档子系统单独存） */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FTASavedClothing Inner;

	/** Definition 为空表示没穿外套 */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FTASavedClothing Outer;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TArray<FTASavedSlot> StorySlots;
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

	/** 界面里剧情格的下标编码：剧情槽 i 对应 StorySlotUIIndexBase - i，与口袋格的扁平下标区分 */
	static constexpr int32 StorySlotUIIndexBase = -100;
	static int32 ToStorySlotUIIndex(int32 StorySlotIndex) { return StorySlotUIIndexBase - StorySlotIndex; }
	static bool IsStorySlotUIIndex(int32 UIIndex) { return UIIndex <= StorySlotUIIndexBase && UIIndex > StorySlotUIIndexBase - StorySlotCount; }

	/** 剧情物品（QuestItem）转交 TryAddStoryItem，其余放进口袋。返回实际加入数。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 TryAddItem(UTAItemDefinition* ItemDef, int32 Count = 1);

	/**
	 * 剧情物品按 StorySlotIndex 入槽，每槽 1 个。槽里是 UpgradesFrom 指向的物品时直接替换。
	 * 未绑定、槽被其他物品占用时拒绝并打 Warning；已经持有（含已持有其升级版）时返回 0。
	 * 回滚记录期间会记录撤销（恢复槽里原来的物品）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Story")
	int32 TryAddStoryItem(UTAItemDefinition* ItemDef);

	/** 剧情槽里是否有该物品；槽里是它的升级版也算持有 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Story")
	bool HasStoryItem(const UTAItemDefinition* ItemDef) const;

	/** 扁平下标：仅针对 GetSlots()，不是剧情格 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveFromSlot(int32 SlotIndex, int32 Count = 1);

	/** 按物品定义移除最多 Count 个（普通格，不含剧情格），返回实际移除数。用于回滚等不关心具体格子的场合。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItem(UTAItemDefinition* ItemDef, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemCount(UTAItemDefinition* ItemDef) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsSlotEmpty(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasEmptySlot() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool DropFromSlot(int32 SlotIndex, int32 Count, FTAInventorySlot& OutDropped);

	/**
	 * 把口袋格子里的 Count 个物品丢到地上（策划文档的落点规则）：
	 * 先试脚下；脚下已有物品时，从角色正前方开始每 60° 试一个方向。
	 * 被墙挡住、高低差超过 DropMaxStepHeight、落点与角色夹角超过 DropMaxSlopeAngle、附近已有物品的位置不能落。
	 * 都不能落时返回 false，物品留在原格子。剧情格不能丢。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop")
	bool DropSlotToWorld(int32 SlotIndex, int32 Count, AActor* Dropper);

	/** 找一个能放下物品的位置（地面上的点）；找不到返回 false */
	bool FindDropLocation(const AActor* Dropper, FVector& OutLocation) const;

	/** 脚下以外的落点离角色中心的水平距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "10"))
	float DropDistance = 80.f;

	/** 落点附近这个半径内已有掉落物时，视为“已有物品” */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "1"))
	float DropClearRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0"))
	float DropMaxStepHeight = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0", ClampMax = "89"))
	float DropMaxSlopeAngle = 45.f;

	/** 生成的掉落物离地高度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop")
	float DropSpawnHeight = 10.f;

	/**
	 * 拆分堆叠：从 SlotIndex 拿出 Count 个（1 ~ 全部）放进第一个空格，不与其他同类堆叠合并。
	 * 没有空格、剧情格、数量不合法时返回 false，什么都不改。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SplitStack(int32 SlotIndex, int32 Count);

	/** Move, swap, or merge a complete inventory stack between two UI slot indices. 剧情格不参与，一律返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItemBetweenSlots(int32 SourceSlotIndex, int32 TargetSlotIndex);

	/** 装备外套（Layer 必须是 Outer） */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Clothing")
	bool EquipOuter(UTAClothingDefinition* OuterDef);

	/** 卸下外套；OutInstance 带出口袋里的物品，由外部生成世界物 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Clothing")
	bool UnequipOuter(FTAClothingInstance& OutInstance);

	/** 存档：按格子位置保存所有口袋、外套和剧情槽 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Save")
	FTAInventorySaveData GetSaveData() const;

	/** 读档：整体覆盖。资产找不到的物品丢弃并打 Warning；衣服口袋布局变了时多出的格子丢弃 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Save")
	void RestoreSaveData(const FTAInventorySaveData& Data);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void NotifyUpdated();

	/** 手动整理：合并同类堆叠并排序。拾取 / 移除 / 丢弃不再自动调用 */
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

	/** 在指定衣服实例上尝试加入，返回实际加入数。bFillStacks：补已有同类堆叠；bFillEmpty：放进空格 */
	int32 TryAddItemToClothing(FTAClothingInstance& Clothing, UTAItemDefinition* ItemDef, int32 Count, bool bFillStacks = true, bool bFillEmpty = true);

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
