#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Story/TADialogueTypes.h"
#include "Document/TADocumentSubsystem.h"
#include "Inventory/TAInventoryComponent.h"
#include "TASaveGame.generated.h"

/**
 * 一份存档。各系统各自提供一块数据，由 UTASaveSubsystem 收集和回写。
 * 结构变化时提升 DefaultGame.ini 的 SaveFormatVersion，并在读档时做逐级迁移（见 Docs/VersionManagement.md）。
 */
UCLASS()
class THE_AWAKENING_API UTASaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 写入时的存档格式版本；读到比当前程序更高的版本时拒绝读档 */
	UPROPERTY()
	int32 SaveFormatVersion = 1;

	/** 写入时的游戏完整版本（版本 + 构建 ID），只做记录 */
	UPROPERTY()
	FString GameVersion;

	UPROPERTY()
	FDateTime SaveTime;

	/** 关卡短名（不含 PIE 前缀），读档时不同则先打开这个关卡 */
	UPROPERTY()
	FString LevelName;

	/** 存档时的复活点，读档后角色出现在这里 */
	UPROPERTY()
	FName CheckpointId;

	UPROPERTY()
	FTAStorySaveData Story;

	UPROPERTY()
	FTADocumentSaveData Documents;

	UPROPERTY()
	FTAInventorySaveData Inventory;

	UPROPERTY()
	int32 Money = 0;
};
