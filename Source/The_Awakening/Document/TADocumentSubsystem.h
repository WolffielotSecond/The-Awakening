#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TADocumentSubsystem.generated.h"

/** 文本收集记录的存档数据 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTADocumentSaveData
{
	GENERATED_BODY()

	/** 已收集的文本，按收集顺序（即日记本页序） */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TArray<FName> CollectedDocuments;

	/** 读过的文本，含未获得日记本时读过、没有收集的 */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TArray<FName> ReadDocuments;

	/** 日记本上次停留的页码 */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 DiaryPageIndex = 0;
};

class UTADocumentDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTAOnDocumentCollected, FName, DocumentId, int32, PageIndex);

/**
 * 文本收集记录的唯一持有者：收集顺序、已读旗标、日记本页码。
 * 只存 DocumentId，不引用文本资产；界面需要标题、模型时再按 ID 查 UTADocumentDefinition。
 * 回滚记录期间（对话、小游戏中）的每一项改动都调用 RecordUndo，死亡时随作用域一起撤销。
 */
UCLASS()
class THE_AWAKENING_API UTADocumentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UTADocumentSubsystem* Get(const UObject* WorldContext);

	/** 收集一份文本：追加到日记本末页，同时标记已读。已收集或 ID 为空时返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "Document")
	bool Collect(FName DocumentId);

	/** 标记已读（未获得日记本时阅读走这里，不收集）。已读过或 ID 为空时返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "Document")
	bool MarkRead(FName DocumentId);

	UFUNCTION(BlueprintPure, Category = "Document")
	bool IsCollected(FName DocumentId) const;

	UFUNCTION(BlueprintPure, Category = "Document")
	bool IsRead(FName DocumentId) const;

	/** 日记本页数 = 已收集文本数 */
	UFUNCTION(BlueprintPure, Category = "Document")
	int32 GetPageCount() const { return CollectedDocuments.Num(); }

	/** 第 PageIndex 页的文本 ID；越界返回 NAME_None */
	UFUNCTION(BlueprintPure, Category = "Document")
	FName GetDocumentAtPage(int32 PageIndex) const;

	/** 文本所在页码；未收集返回 INDEX_NONE */
	UFUNCTION(BlueprintPure, Category = "Document")
	int32 GetPageOfDocument(FName DocumentId) const;

	UFUNCTION(BlueprintPure, Category = "Document")
	TArray<FName> GetCollectedDocuments() const { return CollectedDocuments; }

	UFUNCTION(BlueprintPure, Category = "Document")
	int32 GetDiaryPageIndex() const { return DiaryPageIndex; }

	/** 设置日记本页码，限制在 [0, 页数-1]；没有文本时为 0 */
	UFUNCTION(BlueprintCallable, Category = "Document")
	void SetDiaryPage(int32 PageIndex);

	/** 收集成功后广播，PageIndex 为新文本所在页 */
	UPROPERTY(BlueprintAssignable, Category = "Document")
	FTAOnDocumentCollected OnDocumentCollected;

	/** 按 DocumentId 找文本定义（在资产注册表里查 UTADocumentDefinition，结果缓存）。找不到返回 nullptr 并打 Warning。 */
	UFUNCTION(BlueprintCallable, Category = "Document")
	UTADocumentDefinition* FindDefinition(FName DocumentId);

	// ==================== 存档 ====================

	UFUNCTION(BlueprintCallable, Category = "Document|Save")
	FTADocumentSaveData GetSaveData() const;

	/** 读档时整体覆盖；空 ID 和重复项会被丢弃，页码重新限制范围。不记录回滚。 */
	UFUNCTION(BlueprintCallable, Category = "Document|Save")
	void RestoreSaveData(const FTADocumentSaveData& SaveData);

private:
	int32 ClampPage(int32 PageIndex) const;
	void RebuildDefinitionCache();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTADocumentDefinition>> DefinitionCache;

	TArray<FName> CollectedDocuments;
	TSet<FName> ReadDocuments;
	int32 DiaryPageIndex = 0;
};
