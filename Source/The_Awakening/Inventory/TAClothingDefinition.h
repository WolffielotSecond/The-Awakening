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
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	/** 内衬或外套 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	ETAClothingLayer Layer = ETAClothingLayer::Inner;

	/** 本地化 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	FString DisplayNameId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing")
	FString DescriptionId;

	/** 装备栏、衣架交换栏和拖拽预览使用的小图标。与衣服展示图独立。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing", meta = (DisplayName = "Equipment Icon"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** 背包和换衣界面右侧的完整穿着图。外套美术可包含露出的内衣；不读取 Equipment Icon。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing", meta = (DisplayName = "Clothing Display Texture"))
	TSoftObjectPtr<UTexture2D> DisplayTexture;

	/** 仅外套：衣架换衣界面左侧展示的外套单独立绘，不包含内衣。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing", meta = (DisplayName = "Rack Display Texture", EditCondition = "Layer == ETAClothingLayer::Outer", EditConditionHides))
	TSoftObjectPtr<UTexture2D> RackDisplayTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Common/T_DefaultItemIcon.T_DefaultItemIcon")));

	/** 口袋数量；调整后自动增减下面的口袋配置。口袋按列表顺序从上到下显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clothing", meta = (ClampMin = "1", ClampMax = "3", UIMin = "1", UIMax = "3"))
	int32 PocketCount = 1;

	/** 每个口袋 1–2 行、1–4 列。数量由 PocketCount 控制；旧资产容量在加载时保留。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, EditFixedSize, Category = "Clothing", meta = (TitleProperty = "PocketId"))
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
	UPROPERTY(BlueprintReadOnly, Category = "Clothing|UI")
	bool bOverrideInnerPocketDisplay = false;

	/** 按内衬口袋顺序一一对应；数量不够的口袋保持内衬自己的配置 */
	UPROPERTY(BlueprintReadOnly, Category = "Clothing|UI")
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
