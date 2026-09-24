// Source/The_Awakening/Story/TADialogueController.h
// 单个对话会话的状态机（UI 无关：运行时对话 UI 与编辑器预览共用）
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Story/TADialogueTypes.h"
#include "Story/TADialogueConditions.h"
#include "TADialogueController.generated.h"

class UTADialogueSubsystem;

UCLASS()
class THE_AWAKENING_API UTADialogueController : public UObject
{
	GENERATED_BODY()

public:
	UTADialogueController();

	/** 绑定子系统与剧情数据（Start() 前调用） */
	void Initialize(UTADialogueSubsystem* InSubsystem, const FTAStoryData& InStory, UObject* InInitiator);

	/** 进入入口节点，正式启动 */
	void Start();

	/** 会话结束清理（解绑委托等） */
	void Shutdown();

	/** 每帧驱动（打字机进度）；由 UI 的 NativeTick 调用 */
	void Tick(float DeltaTime);

	// ==================== 玩家操作 ====================

	/** 继续：打字中→立即完成打字；否则→下一句（分支界面无效） */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Advance();

	/** 选择第 VisibleIndex 个可见选项 */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SelectChoice(int32 VisibleIndex);

	/** 移动选项高亮（Delta ±1，循环） */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void MoveSelection(int32 Delta);

	/** 更新当前高亮项，不触发选项列表重建。 */
	void SetSelectedChoiceIndex(int32 VisibleIndex);

	// ==================== 状态查询（UI 渲染用） ====================

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsTyping() const { return bTyping; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsChoiceNode() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FText GetFullText() const { return CurrentFullText; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FText GetVisibleText() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FText GetSpeakerName() const { return CurrentSpeakerName; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FString GetSpeakerId() const { return SpeakerId; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FString GetStoryId() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	TArray<FTAPortraitEntry> GetPortraits() const { return ActivePortraits; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	TArray<FTAStoryHistoryEntry> GetHistory() const { return HistoryEntries; }

	/** 当前可见选项（对应 Node.Choices 的真实下标） */
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	TArray<int32> GetVisibleChoiceIndices() const { return VisibleChoiceIndices; }

	/** 当前可见选项文本（按当前语言解析） */
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	TArray<FText> GetVisibleChoiceTexts() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	int32 GetSelectedChoiceIndex() const { return SelectedChoiceIndex; }

	// ==================== 编辑器预览 ====================

	/** 编辑器预览注入模拟状态（bUseOverrides=true 时条件用模拟数据） */
	void SetConditionOverrides(const FTADialogueConditionContext& InOverrides);

	// ==================== 委托（UI 绑定） ====================

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStoryLineShown OnLineShown;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStoryChoicesChanged OnChoicesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStoryPortraitsChanged OnPortraitsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStorySpeakerChanged OnSpeakerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStoryHistoryChanged OnHistoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStoryFinished OnStoryFinished;

protected:
	void EnterNode(int32 NodeIndex);
	void BeginLine(const FTAStoryNode& Node);
	void CompleteLine();
	void GoToNext();
	void PresentChoices(const FTAStoryNode& Node);
	void Finish();

	void ApplyPortraitEntries(const TArray<FTAPortraitEntry>& Entries);
	void RunEvents(const TArray<FTAStoryEvent>& Events);
	void NotifyProgress();

	FTADialogueConditionContext BuildContext() const;
	FText ResolveText(const FString& TextId) const;
	bool AllConditionsTrue(const TArray<FTAStoryCondition>& Conditions) const;

	UFUNCTION()
	void HandleLanguageChanged();

protected:
	UPROPERTY()
	TObjectPtr<UTADialogueSubsystem> Subsystem;

	const FTAStoryData* Story = nullptr;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UObject> Initiator;

	bool bActive = false;
	int32 CurrentNodeIndex = INDEX_NONE;

	// 打字机
	bool bTyping = false;
	float VisibleCharCount = 0.f;
	float CharsPerSecond = 40.f;
	FText CurrentFullText;
	FText CurrentSpeakerName;

	// 立绘运行时状态（按 CharacterId 键控）
	TArray<FTAPortraitEntry> ActivePortraits;

	// 分支
	TArray<int32> VisibleChoiceIndices;
	int32 SelectedChoiceIndex = 0;

	// 历史
	TArray<FTAStoryHistoryEntry> HistoryEntries;

	// 说话者
	FString SpeakerId;

	// 编辑器预览模拟数据
	FTADialogueConditionContext OverrideContext;
	bool bOverrideContext = false;
};
