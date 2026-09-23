// Source/The_Awakening/Story/TADialogueWidget.h
// 对话主 UI（WBP_Dialogue 的 C++ 基类）
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Story/TADialogueTypes.h"
#include "TADialogueWidget.generated.h"

class UTADialogueController;
class UTADialogueSubsystem;
class UTAPortraitWidget;
class UTADialogueChoiceButton;
class UTADialogueHistoryWidget;
class UTALocalizeSubsystem;
class UTAInputIconSubsystem;
class UTextBlock;
class UButton;
class UImage;
class UCanvasPanel;
class UVerticalBox;
class UPanelWidget;
class UInputAction;
class UInputMappingContext;

/**
 * 控件命名约定（WBP_Dialogue，布局全部在编辑器里摆，C++ 只绑定逻辑）：
 * 必需：
 *   Text_Name          UTextBlock   角色名字框
 *   Text_Dialogue      UTextBlock   对话框
 *   Button_Continue    UButton      继续按钮
 *   Button_History     UButton      历史按钮
 *   Canvas_Portraits   UCanvasPanel 立绘层（充满立绘显示区域）
 *   Box_Choices        UVerticalBox 选项列表容器
 * 可选：
 *   Panel_Choices      UPanelWidget 分支面板整体（显隐用；缺省时用 Box_Choices 自己）
 *   Widget_History     UTADialogueHistoryWidget（WBP_DialogueHistory 实例，历史面板）
 *   Image_ContinueIcon UImage       继续键图标（热切换显示）
 *   Image_HistoryIcon  UImage       历史键图标（热切换显示）
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 由子系统调用：绑定子系统与控制器 */
	void Setup(UTADialogueSubsystem* InSubsystem, UTADialogueController* InController);

	/** 关闭并移除 UI（子系统 StopDialogue 调用） */
	void CloseDialogue();

	/** 编辑器预览模式：结束时不动子系统会话 */
	void SetPreviewMode(bool bInPreviewMode) { bPreviewMode = bInPreviewMode; }

	// ==================== 输入（蓝图/按钮/输入事件共用入口） ====================

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void OnAdvancePressed();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void ToggleHistory();

protected:
	// ==================== 渲染刷新 ====================
	void RefreshLine();
	void RefreshChoices();
	void RefreshPortraits();
	void RefreshHistory();
	void RefreshIcons();
	void UpdateTalkingFlags();

	UFUNCTION()
	void HandleLineShown();
	UFUNCTION()
	void HandleChoicesChanged();
	UFUNCTION()
	void HandlePortraitsChanged();
	UFUNCTION()
	void HandleSpeakerChanged();
	UFUNCTION()
	void HandleHistoryChanged();
	UFUNCTION()
	void HandleStoryFinished();
	UFUNCTION()
	void HandleLanguageChanged();
	UFUNCTION()
	void HandleInputDeviceChanged();

	UFUNCTION()
	void OnChoiceClicked(int32 Index);

	// ==================== 输入管理 ====================
	void BindInputActions();
	void UnbindInputActions();
	void PushDialogueMappingContext();
	void PopDialogueMappingContext();

	// ==================== 输入配置（WBP 上指定） ====================

	UPROPERTY(EditAnywhere, Category = "Dialogue|Input")
	TObjectPtr<UInputAction> AdvanceAction;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Input")
	TObjectPtr<UInputAction> HistoryAction;

	/** 对话期间激活的映射上下文（高优先级屏蔽移动等） */
	UPROPERTY(EditAnywhere, Category = "Dialogue|Input")
	TObjectPtr<UInputMappingContext> DialogueMappingContext;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Input")
	int32 DialogueMappingPriority = 10;

	// ==================== 绑定控件 ====================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Dialogue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Continue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_History;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Canvas_Portraits;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Choices;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Choices;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTADialogueHistoryWidget> Widget_History;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ContinueIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_HistoryIcon;

	// ==================== 动态创建 ====================

	UPROPERTY()
	TArray<TObjectPtr<UTAPortraitWidget>> PortraitWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UTADialogueChoiceButton>> ChoiceWidgets;

	UPROPERTY()
	TObjectPtr<UTADialogueController> Controller;

	UPROPERTY()
	TObjectPtr<UTADialogueSubsystem> Subsystem;

	UPROPERTY()
	TObjectPtr<UTALocalizeSubsystem> LocalizeSubsystem;

	UPROPERTY()
	TObjectPtr<UTAInputIconSubsystem> InputIconSubsystem;

	/** 输入绑定句柄（解绑用） */
	TArray<uint32> InputBindingHandles;

	bool bHistoryOpen = false;
	bool bPreviewMode = false;
	bool bMappingPushed = false;
};
