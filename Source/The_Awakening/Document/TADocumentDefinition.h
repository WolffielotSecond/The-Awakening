#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TADocumentDefinition.generated.h"

class UStaticMesh;
class UTexture2D;
class UTADocumentPresentation;

/**
 * 一份可收集文本（信件、笔记等）。
 * 文本不是物品：不占背包格子、不能丢弃、不能堆叠，收集记录由 UTADocumentSubsystem 单独持有。
 * 标题和正文存本地化 ID，走 TALocalizeSubsystem；正文用 RichText 标记 <img id="key"/> 插图，key 查 InlineImages。
 */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTADocumentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 唯一 ID，存档和剧情条件（HasDocument / ReadDocument）用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	FName DocumentId;

	/** 标题的本地化 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	FString TitleTextId;

	/** 正文的本地化 ID；正文可含 <img id="key"/> 标记 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	FString BodyTextId;

	/** 拾取列表中显示的图标（可空） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 扫描悬停时显示的介绍（本地化 ID） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	FString ScanDescriptionTextId;

	/** 正文插图：<img id="key"/> 中的 key → 图片 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	TMap<FName, TSoftObjectPtr<UTexture2D>> InlineImages;

	/** 场景、拾取提示、日记本、文本预览共用一个模型；骨骼模型可通过 Presentation 配置动画，为空时显示占位模型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	TObjectPtr<UObject> Mesh = nullptr;

	/** Optional shared axis correction and folding animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UTADocumentPresentation> Presentation;

	FTransform GetPreviewTransform(const FTransform& Adjustment) const;

	/** 场景中模型相对文本 Actor 的偏移、旋转、缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FTransform WorldTransform = FTransform::Identity;

	/** 拾取提示预览台中的偏移、旋转、缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FTransform PickupPreviewTransform = FTransform::Identity;

	/** 文本预览界面（阅读 / 检视）中的偏移、旋转、缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FTransform ReaderTransform = FTransform::Identity;

	/** 日记本页面中的偏移、旋转、缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FTransform DiaryPageTransform = FTransform::Identity;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TADocument"), GetFName());
	}
};
