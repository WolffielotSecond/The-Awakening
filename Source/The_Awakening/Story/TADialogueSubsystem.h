// Source/The_Awakening/Story/TADialogueSubsystem.h
// 剧情子系统（GameInstance 级，跨关卡）
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Story/TADialogueTypes.h"
#include "Story/TADialogueConditions.h"
#include "Health/TADamageTypes.h"
#include "Health/TARollbackSubsystem.h"
#include "Story/TAWorldEventDefinition.h"
#include "TADialogueSubsystem.generated.h"

class UTADialogueController;
class UTADialogueWidget;
class UTADialoguePortraitLayerWidget;
class UTAPortraitWidget;
class UTADialogueChoiceButton;
class UTAStoryAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTAWorldEventChanged, FName, EventId, ETAWorldEventResult, Result, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTAWorldStateChanged);

/**
 * 职责：剧情 JSON 加载缓存、旗标存储、条件注册表、事件广播、
 * 对话会话管理、存档接口、JSON 读写（编辑器复用同一实现）。
 * 配置在 DefaultGame.ini [/Script/The_Awakening.TADialogueSubsystem]。
 */
UCLASS(Config = Game, DefaultConfig)
class THE_AWAKENING_API UTADialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

 UPROPERTY(BlueprintAssignable, Category="Story|World") FTAWorldEventChanged OnWorldEventChanged;
 UPROPERTY(BlueprintAssignable, Category="Story|World") FTAWorldStateChanged OnWorldStateChanged;
 UFUNCTION(BlueprintPure, Category="Story|World") FGameplayTagContainer GetWorldStateTags() const { return WorldStateTags; }
 UFUNCTION(BlueprintCallable, Category="Story|World") void SetWorldStateTag(FGameplayTag Tag, bool bEnabled);
 UFUNCTION(BlueprintPure, Category="Story|World") bool CanStartWorldEvent(UTAWorldEventDefinition* Definition, AActor* Interactor) const;
 UFUNCTION(BlueprintCallable, Category="Story|World") bool StartWorldEvent(UTAWorldEventDefinition* Definition, AActor* Interactor, UObject* Executor);
 /** Only the executor that started this event may finish it. */
 UFUNCTION(BlueprintCallable, Category="Story|World") bool FinishWorldEvent(FName EventId, ETAWorldEventResult Result, UObject* Executor);
 UFUNCTION(BlueprintPure, Category="Story|World") bool IsWorldEventActive(FName EventId) const { return ActiveWorldEvents.Contains(EventId); }
 void CancelWorldEventsFor(UObject* Executor);
 void CancelAllWorldEvents();
 void RecordWorldStateUndo();
 UFUNCTION(BlueprintPure, Category="Story|World") bool HasCompletedWorldEvent(FName EventId) const { return CompletedWorldEvents.Contains(EventId); }
 UFUNCTION(BlueprintPure, Category="Story|World") FName GetWorldBranch(FName Group) const { return WorldBranches.FindRef(Group); }

	// ==================== 旗标 ====================

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Flag")
	int32 GetFlagInt(const FString& Flag) const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Flag")
	void SetFlagInt(const FString& Flag, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Flag")
	bool GetFlagBool(const FString& Flag) const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Flag")
	void SetFlagBool(const FString& Flag, bool bValue);

	// ==================== 条件 ====================

	bool EvaluateCondition(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const;

	/** 注册自定义条件类型（C++ 扩展接口；类型名冲突时覆盖） */
	void RegisterConditionEvaluator(TSharedPtr<ITADialogueConditionEvaluator> Evaluator);

	// ==================== 事件广播 ====================

	/** 事件广播（BlueprintAssignable）：监听后按 Payload.Name 分发，Payload.Params 取参数 */
	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Event")
	FOnStoryEvent OnStoryEvent;

	/** 触发事件：先执行同名保留处理器（内部状态，如 SetFlag），再广播给所有监听者 */
	UFUNCTION(BlueprintCallable, Category = "Dialogue|Event")
	void BroadcastStoryEvent(FName EventName, const TMap<FString, FString>& Params);

	/** 注册保留名处理器（C++ 扩展接口） */
	void RegisterReservedEventHandler(FName EventName, TFunction<void(const FTAStoryEventPayload&)> Handler);

	// ==================== 对话会话 ====================

	/** Start a dialogue from a compiled Unreal story asset. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartDialogueAsset(UTAStoryAsset* StoryAsset, UObject* Initiator = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StopDialogue();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	UTADialogueController* GetActiveController() const { return ActiveController; }

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool IsDialogueActive() const { return ActiveController != nullptr; }

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueEnded OnDialogueEnded;

	// ==================== 存档接口（未来存档系统调用） ====================

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Save")
	FTAStorySaveData GetSaveData() const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Save")
	void RestoreSaveData(const FTAStorySaveData& SaveData);

	/** 内部：控制器进度回写 */
	void UpdateStoryProgress(const FString& StoryId, const FString& NodeId);
	void MarkStoryCompleted(const FString& StoryId);

	// Legacy JSON parsing helpers are retained for compatibility only; playback now uses story assets.

	static bool ParseStoryJson(const FString& JsonString, const FString& SourceName, FTAStoryData& OutData, FString& OutError);
	static bool StoryToJson(const FTAStoryData& Story, FString& OutJson);

	/** 单字段 JSON 字符串化（bool/number/string → string），解析辅助函数使用 */
	static FString JsonScalarToString(const TSharedPtr<FJsonValue>& Value);

	// ==================== 配置 ====================

	/** 对话 UI 蓝图类（WBP_Dialogue） */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue|UI")
	TSoftClassPtr<UTADialogueWidget> DialogueWidgetClass;

	/** 立绘控件蓝图类（WBP_Portrait） */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue|UI")
	TSoftClassPtr<UTAPortraitWidget> PortraitWidgetClass;

	/** 选项按钮蓝图类（WBP_DialogueChoiceButton） */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue|UI")
	TSoftClassPtr<UTADialogueChoiceButton> ChoiceButtonWidgetClass;

	/** 打字机速度（字符/秒） */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue|Typewriter")
	float CharsPerSecond = 40.f;

protected:
	bool StartDialogueFromData(const FTAStoryData& Story, UObject* Initiator);

	/** 保留名处理器：SetFlag */
	void HandleReservedSetFlag(const FTAStoryEventPayload& Payload);

protected:
	UPROPERTY()
 FGameplayTagContainer WorldStateTags;
 TSet<FName> CompletedWorldEvents;
 TMap<FName, FName> WorldBranches;
 struct FActiveWorldEvent
 {
  TWeakObjectPtr<UTAWorldEventDefinition> Definition;
  TWeakObjectPtr<AActor> Interactor;
  TWeakObjectPtr<UObject> Executor;
 };
 TMap<FName, FActiveWorldEvent> ActiveWorldEvents;
 bool bCancellingWorldEvents = false;
	TMap<FString, int32> Flags;

	UPROPERTY()
	TMap<FString, FString> StoryProgress;

	UPROPERTY()
	TArray<FString> CompletedStories;

	UPROPERTY()
	TObjectPtr<UTADialogueController> ActiveController;

	UPROPERTY()
	TObjectPtr<UTADialogueWidget> ActiveWidget;

	UPROPERTY()
	TObjectPtr<UTADialoguePortraitLayerWidget> ActivePortraitLayer;

	UPROPERTY()
	TObjectPtr<UTAStoryAsset> ActiveStoryAsset;

	TArray<TSharedPtr<ITADialogueConditionEvaluator>> ConditionEvaluators;
	TMap<FName, TFunction<void(const FTAStoryEventPayload&)>> ReservedHandlers;

private:
	/** 对话期间：玩家无敌 + 不会死；回滚日志记录对话做的所有改动，死亡时撤销并强制结束对话。 */
	void BeginDialogueGuards(class AThe_AwakeningCharacter* PlayerCharacter);
	void EndDialogueGuards();

	FTARollbackScopeHandle DialogueRollbackScope;
	FTAHealthProtectionHandle DialogueInvulnerable;
	FTAHealthProtectionHandle DialogueUndying;
	TWeakObjectPtr<class UTAHealthComponent> ProtectedHealth;
};
