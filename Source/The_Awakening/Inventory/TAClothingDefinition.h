#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/TAClothingTypes.h"
#include "GameplayTagContainer.h"
#include "TAClothingDefinition.generated.h"

class UTexture2D;
class UGameplayEffect;

/** 衣服定义 */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTAClothingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 内衬或外套 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	ETAClothingLayer Layer = ETAClothingLayer::Inner;

	/** 本地化 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	FString DisplayNameId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	FString DescriptionId;

	/** UI 上的衣服图 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 口袋列表；格子总数 = 各口袋格子数（Columns * Rows，旧资产为 SlotCount）之和 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	TArray<FTAPocketDef> Pockets;

	/**
	 * 穿着期间生效的 GE（通过拥有者的 ASC 施加）：穿上 / 换上 / 读档时施加，脱下 / 换下时移除。
	 * 持续型、无限型会随衣服移除；瞬时型只在穿上时生效一次，脱下不撤销。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing|Abilities")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	/** 这件衣服的专属能力。穿着期间作为松散 Tag 加在拥有者的 ASC 上，供技能等系统查询 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing|Abilities")
	FGameplayTagContainer ExclusiveAbilityTags;

	/** 仅外套：穿上后是否改写内衬（默认）口袋在界面上的位置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing|UI", meta = (EditCondition = "Layer == ETAClothingLayer::Outer"))
	bool bOverrideInnerPocketDisplay = false;

	/** 按内衬口袋顺序一一对应；数量不够的口袋保持内衬自己的配置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing|UI", meta = (EditCondition = "Layer == ETAClothingLayer::Outer && bOverrideInnerPocketDisplay"))
	TArray<FTAPocketDisplayOverride> InnerPocketDisplayOverrides;

	/** 总格子数 */
	UFUNCTION(BlueprintCallable, Category = "Clothing")
	int32 GetTotalSlotCount() const
	{
		int32 Total = 0;
		for (const FTAPocketDef& Pocket : Pockets)
		{
			Total += Pocket.GetSlotCount();
		}
		return Total;
	}
};