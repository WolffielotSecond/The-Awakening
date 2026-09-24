// Source/The_Awakening/Story/TADialogueSubsystem.h
// 剧情子系统（GameInstance 级，跨关卡）
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Story/TADialogueTypes.h"
#include "Story/TADialogueConditions.h"
#include "TADialogueSubsystem.generated.h"

class UTADialogueController;
class UTADialogueWidget;
class UTADialoguePortraitLayerWidget;
class UTAPortraitWidget;
class UTADialogueChoiceButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEnded);

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

	// ==================== 剧情加载 ====================

	/** 加载并缓存剧情；失败返回 nullptr */
	const FTAStoryData* LoadStory(const FString& StoryId);

	/** 剧情文件完整路径 */
	FString GetStoryFilePath(const FString& StoryId) const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool IsStoryLoaded(const FString& StoryId) const { return LoadedStories.Contains(StoryId); }

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

	/** 开始一段对话（需要 DefaultGame.ini 配置 DialogueWidgetClass） */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartDialogue(const FString& StoryId, UObject* Initiator = nullptr);

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

	// ==================== JSON 读写（编辑器复用同一实现） ====================

	static bool ParseStoryJson(const FString& JsonString, const FString& SourceName, FTAStoryData& OutData, FString& OutError);
	static bool StoryToJson(const FTAStoryData& Story, FString& OutJson);

	/** 单字段 JSON 字符串化（bool/number/string → string），解析辅助函数使用 */
	static FString JsonScalarToString(const TSharedPtr<FJsonValue>& Value);

	// ==================== 配置 ====================

	/** 剧情 JSON 所在 Content 相对目录 */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue")
	FString StoriesFolder = TEXT("Stories");

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
	/** 保留名处理器：SetFlag */
	void HandleReservedSetFlag(const FTAStoryEventPayload& Payload);

protected:
	UPROPERTY()
	TMap<FString, FTAStoryData> LoadedStories;

	UPROPERTY()
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

	TArray<TSharedPtr<ITADialogueConditionEvaluator>> ConditionEvaluators;
	TMap<FName, TFunction<void(const FTAStoryEventPayload&)>> ReservedHandlers;
};
