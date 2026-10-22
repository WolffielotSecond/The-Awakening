#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TAItemTypes.h"
#include "GameplayTagContainer.h"
#include "TAItemDefinition.generated.h"

class UStaticMesh;
class ATAWorldItem;

UCLASS(BlueprintType)
class THE_AWAKENING_API UTAItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Historical acquisition state, granted only after a successful inventory addition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Story")
	FGameplayTagContainer GrantedStoryTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	ETAItemType ItemType = ETAItemType::Treasure;

	/** 是否可堆叠 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bCanStack = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "bCanStack", ClampMin = "1"))
	int32 MaxStack = 1;

	/** 卖出/商店参考价 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 Value = 0;

	/** 是否可卖出（商店逻辑后接） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bCanSell = false;

	/** 拾取提示和检视界面共用的 3D 模型；为空时显示占位立方体 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UStaticMesh> PreviewMesh = nullptr;

	/** 丢弃到地上时生成的世界物品（例如 BP_WorldItem_Herb）；为空时生成基础 TAWorldItem，模型用 PreviewMesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TSoftClassPtr<ATAWorldItem> WorldItemClass;

	/** 模型在预览台中的偏移、旋转、缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	FTransform PreviewTransform = FTransform::Identity;

	/** 消耗品：使用时数量-1；可在此挂恢复量等（效果后做） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	float ConsumableMagnitude = 0.f;

	/** 武器 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float WeaponDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float MaxDurability = 100.f;

	/** 任务/门卡：标识用，开门逻辑后接 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestItemId;

	/** 剧情物品绑定的剧情槽（0~3）；-1 表示未绑定，拾取时会被拒绝 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (EditCondition = "ItemType == ETAItemType::QuestItem", ClampMin = "-1", ClampMax = "3"))
	int32 StorySlotIndex = -1;

	/** 升级版剧情物品：入槽时替换槽里的这个物品（同一个剧情槽） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (EditCondition = "ItemType == ETAItemType::QuestItem"))
	TObjectPtr<UTAItemDefinition> UpgradesFrom = nullptr;

	/** 技能书：解锁哪棵树（原型只用 Id） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SkillBook")
	FName SkillTreeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bIsCurrency = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TAItem"), GetFName());
	}
};