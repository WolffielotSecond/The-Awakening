#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TASaveSubsystem.generated.h"

class UTASaveGame;
class UWorld;

/**
 * 最小存档层：一个存档槽，复活点激活时自动存，读档由（以后的）主菜单或调试命令触发。
 *
 * 存：存档头（格式版本、游戏版本、时间、关卡、复活点）+ 剧情旗标与进度 + 文本收集记录 + 背包（按格子位置）+ 钱。
 * 读：恢复以上数据，角色传送到存档的复活点，场景里已收集的文本销毁；关卡不同时先打开存档关卡，角色生成后再恢复。
 * 读到比当前程序更高的存档格式时拒绝读档，不覆盖原存档。
 *
 * 已知限制（最小存档层）：场景里普通物品被捡走 / 丢下的状态不存，读档后关卡里的物品回到关卡原样；
 * 生命值不存，读档后为满血。
 */
UCLASS()
class THE_AWAKENING_API UTASaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UTASaveSubsystem* Get(const UObject* WorldContext);

	/** 存到默认槽。读档恢复过程中、没有玩家时不存，返回 false */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveGame();

	/** 从默认槽读档。没有存档、格式版本过高时返回 false */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadGame();

	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasSaveGame() const;

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool DeleteSaveGame();

	/** 读出存档头供界面显示（没有存档返回 nullptr），不应用 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	UTASaveGame* PeekSaveGame() const;

	/** 复活点激活时调用（玩家交互或走进范围），读档恢复中不重复存 */
	void AutoSave();

	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

protected:
	virtual void Deinitialize() override;

private:
	UTASaveGame* BuildSaveGame(UWorld* World) const;
	/** 把存档应用到当前世界；玩家角色还没生成时返回 false */
	bool ApplySaveGame(UWorld* World, UTASaveGame* Save);
	void HandlePostLoadMap(UWorld* World);
	void TryApplyPending();

	UPROPERTY(Transient)
	TObjectPtr<UTASaveGame> PendingLoad;

	FDelegateHandle PostLoadMapHandle;
	FTimerHandle PendingTimer;
	int32 PendingAttempts = 0;
	bool bRestoring = false;
};
