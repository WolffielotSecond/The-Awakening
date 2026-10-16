#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TARollbackSubsystem.generated.h"

DECLARE_DYNAMIC_DELEGATE(FTARollbackUndoDynamicDelegate);

/** 回滚作用域句柄。0 表示无效。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTARollbackScopeHandle
{
	GENERATED_BODY()

	uint64 Id = 0;
	bool IsValid() const { return Id != 0; }
};

/**
 * 回滚日志：对话、小游戏进行期间记录它们做过的每一项改动；玩家死亡时按记录的逆序全部撤销，
 * 再执行作用域的中止动作（强制关闭界面），让它们回到“还没开始”的状态。正常结束时提交，记录直接丢弃。
 *
 * 用法：
 * - 会改游戏状态的代码，在改之前（或之后立刻）调用 RecordUndo，传入能把状态恢复回去的操作。
 *   只在 IsRecording() 时需要记录；不记录也不影响正常流程。
 * - 以后新增的 OnStoryEvent 监听、自定义小游戏奖励接收者，如果会改游戏状态，必须自己调用 RecordUndo（蓝图用 Record Undo by Event）。
 *
 * 作用域可以嵌套（例如对话中打开小游戏），记录写入最近开始的作用域；回滚从最内层到最外层。
 */
UCLASS()
class THE_AWAKENING_API UTARollbackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTARollbackSubsystem* Get(const UObject* WorldContext);

	/**
	 * 开始记录。Owner 销毁视为提交。
	 * @param AbortAction 死亡回滚时，撤销完本作用域的记录后执行，用于强制关闭界面、恢复到未开始状态。
	 */
	FTARollbackScopeHandle BeginScope(UObject* Owner, FName Label, TFunction<void()> AbortAction = nullptr);
	/** 正常结束：丢弃本作用域的记录，不撤销。重复调用或作用域已被回滚时无副作用。 */
	void CommitScope(FTARollbackScopeHandle& Handle);

	UFUNCTION(BlueprintPure, Category = "Rollback")
	bool IsRecording() const;

	/** 记录一条撤销操作，写入最近开始的作用域。没有作用域或正在回滚时忽略。 */
	void RecordUndo(FName Label, TFunction<void()> Undo);
	UFUNCTION(BlueprintCallable, Category = "Rollback", meta = (DisplayName = "Record Undo by Event"))
	void RecordUndoByEvent(FName Label, FTARollbackUndoDynamicDelegate Undo);

	/** 死亡时调用：所有作用域从内到外，各自按逆序撤销记录并执行中止动作。 */
	void RollbackAll();

	int32 GetScopeCount() const;
	int32 GetRecordCount() const;

protected:
	virtual void Deinitialize() override;

private:
	struct FRecord
	{
		FName Label;
		TFunction<void()> Undo;
	};
	struct FScope
	{
		uint64 Id = 0;
		TWeakObjectPtr<UObject> Owner;
		FName Label;
		TArray<FRecord> Records;
		TFunction<void()> AbortAction;
	};

	/** 持有者已销毁的作用域视为已提交。 */
	void PruneScopes() const;

	mutable TArray<FScope> Scopes;
	uint64 NextScopeId = 0;
	bool bRollingBack = false;
};
