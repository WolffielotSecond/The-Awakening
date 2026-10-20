#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TAItemSettings.generated.h"

class UTexture2D;
class UMaterialInterface;
class UMeshComponent;

/**
 * 物品的通用兜底资源。项目设置 → Game → The Awakening Items 中可改，保存在 DefaultGame.ini。
 * 运行时兜底，不修改物品资产：资产里设置了的图标 / 材质始终优先。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "The Awakening Items"))
class THE_AWAKENING_API UTAItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTAItemSettings();

	/** 物品（含文本、衣服）没有设置图标时显示的图标 */
	UPROPERTY(Config, EditAnywhere, Category = "Fallback")
	TSoftObjectPtr<UTexture2D> DefaultItemIcon;

	/** 物品模型材质槽为空或是引擎默认材质（丢失材质）时替换成的材质 */
	UPROPERTY(Config, EditAnywhere, Category = "Fallback")
	TSoftObjectPtr<UMaterialInterface> MissingMaterialFallback;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Icon 为空时返回默认图标 */
	static UTexture2D* ResolveIcon(UTexture2D* Icon);

	/** 把网格体上丢失的材质（槽位为空或引擎默认材质）换成兜底材质；已指定的材质不动 */
	static void ApplyMissingMaterialFallback(UMeshComponent* Mesh);
};
