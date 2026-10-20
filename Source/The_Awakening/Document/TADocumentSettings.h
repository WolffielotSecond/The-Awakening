#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TADocumentSettings.generated.h"

class UTAItemDefinition;
class UTADocumentReaderWidget;

/** 文本系统参数。项目设置 → Game → The Awakening Documents 中可改，保存在 DefaultGame.ini。 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "The Awakening Documents"))
class THE_AWAKENING_API UTADocumentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 日记本物品。玩家剧情槽里有它时，拾取文本直接收集进日记本；没有时只能拿起阅读再放回。 */
	UPROPERTY(Config, EditAnywhere, Category = "Diary")
	TSoftObjectPtr<UTAItemDefinition> DiaryItem;

	/** 所有文本扫描时都显示同一句介绍（UnifiedScanDescriptionTextId），忽略各自的 ScanDescriptionTextId */
	UPROPERTY(Config, EditAnywhere, Category = "Scan")
	bool bUseUnifiedScanDescription = true;

	/** 统一的扫描介绍（本地化 ID） */
	UPROPERTY(Config, EditAnywhere, Category = "Scan", meta = (EditCondition = "bUseUnifiedScanDescription"))
	FString UnifiedScanDescriptionTextId = TEXT("Doc_Scan_Unified");

	/** 阅读界面（WBP_DocumentReader，父类 UTADocumentReaderWidget） */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UTADocumentReaderWidget> ReaderWidgetClass;


	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
