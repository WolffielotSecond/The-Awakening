// Source/The_AwakeningEditor/TADialogueEditorWidget.h
// 剧情编辑器（EUW_StoryEditor 的 C++ 基类）
#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Story/TADialogueTypes.h"
#include "Story/TADialogueConditions.h"
#include "TADialogueRowWidgets.h"
#include "TADialogueEditorWidget.generated.h"

class UButton;
class UScrollBox;
class UVerticalBox;
class UPanelWidget;
class UTextBlock;
class UEditableTextBox;
class UComboBoxString;
class UCheckBox;
class UTADialogueController;
class UTADialogueWidget;
class UTADialogueSubsystem;

/**
 * 控件命名约定（EUW_StoryEditor 蓝图）：
 *   Button_NewStory / Button_Save / Button_Validate / Button_Preview  工具栏按钮
 *   ScrollBox_Stories   剧情文件列表
 *   ScrollBox_Nodes     节点列表（顶部代码生成"添加/删除节点"按钮）
 *   Box_Details         详情面板（代码生成）
 *   Box_Localize        本地化三列编辑区（代码生成）
 *   Box_Simulation      预览模拟状态面板（代码生成）
 *   Box_PreviewHost     预览宿主（UVerticalBox，运行时对话 UI 挂载于此）
 *   Text_Status         状态栏
 */
UCLASS(Blueprintable, meta = (DisplayName = "DialogueEditorWidget（剧情编辑器）"))
class THE_AWAKENINGEDITOR_API UTADialogueEditorWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// ==================== 工具栏 ====================

	UFUNCTION(BlueprintCallable, Category = "DialogueEditor")
	void RefreshStories();

	UFUNCTION(BlueprintCallable, Category = "DialogueEditor")
	void NewStory();

	UFUNCTION(BlueprintCallable, Category = "DialogueEditor")
	void SaveStory();

	UFUNCTION(BlueprintCallable, Category = "DialogueEditor")
	void ValidateStory();

	UFUNCTION(BlueprintCallable, Category = "DialogueEditor")
	void TogglePreview();

	UFUNCTION(BlueprintCallable, Category = "DialogueEditor")
	void ApplySimulation();

protected:
	// ==================== 数据 ====================

	UPROPERTY()
	FTAStoryData EditingStory;

	FString EditingStoryId;
	int32 SelectedNodeIndex = INDEX_NONE;
	FString CurrentLocalizeKey;

	/** 语言 → Key → 文本 */
	TMap<FString, TMap<FString, FString>> LanguageCache;

	/** 扫描到的贴图包路径（立绘下拉选择用） */
	TArray<FString> ScannedTexturePaths;

	// ==================== 预览 ====================

	UPROPERTY()
	TObjectPtr<UTADialogueController> PreviewController;

	UPROPERTY()
	TObjectPtr<UTADialogueWidget> PreviewWidget;

	bool bPreviewActive = false;

	// ==================== 模拟面板控件（代码创建） ====================

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SimMoneyInput;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SimItemsInput;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SimFlagInput;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SimFlagValueInput;

	// ==================== 构建 ====================

	void RebuildStoryList();
	void RebuildNodeList();
	void RebuildDetails();
	void RebuildLocalizeSection();
	void RebuildSimulationPanel();

	void SelectStory(const FString& StoryId);
	void SelectNode(int32 NodeIndex);
	void SetStatus(const FString& Text);
	void ScanTextures();

	// ==================== 详情行工具 ====================

	UTADialogueRowButton* MakeButtonLocal(const FString& Label);
	UTADialogueRowTextBox* AddTextBoxRow(UPanelWidget* Box, const FString& Label, const FString& Initial);
	UTADialogueRowCombo* AddComboRow(UPanelWidget* Box, const FString& Label, const TArray<FString>& Options, const FString& Selected);
	UTADialogueRowCheckBox* AddCheckRow(UPanelWidget* Box, const FString& Label, bool bInitial);
	UTADialogueRowButton* AddButtonRow(UPanelWidget* Box, const FString& Label);
	UTADialogueRowTextBox* AddImagePathRow(UPanelWidget* Box, const FString& Label, const FString& Initial);
	void AddSectionTitle(UPanelWidget* Box, const FString& Title);

	void AddEventRows(UPanelWidget* Box, const FString& Title, TArray<FTAStoryEvent>* Events);
	void AddConditionRows(UPanelWidget* Box, TArray<FTAStoryCondition>* Conditions);
	void AddPortraitRows(UPanelWidget* Box, TArray<FTAPortraitEntry>* Portraits);
	void AddChoiceRows(UPanelWidget* Box, TArray<FTAStoryChoice>* Choices);

	void OpenLocalizeKey(const FString& Key);

	static FString ParamsToString(const TMap<FString, FString>& Params);
	static void ParseParamsString(const FString& Text, TMap<FString, FString>& OutParams);

	// ==================== 校验 ====================

	void CollectValidationErrors(TArray<FString>& OutErrors);

	// ==================== 预览 ====================

	void StartPreview();
	void StopPreview();
	FTADialogueConditionContext BuildPreviewOverrides() const;
	UTADialogueSubsystem* GetDialogueSubsystem() const;

	// ==================== 绑定控件 ====================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_NewStory;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Save;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Validate;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Preview;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Stories;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Nodes;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Details;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Localize;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Simulation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_PreviewHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
};
