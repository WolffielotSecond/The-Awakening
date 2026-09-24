// Source/The_Awakening/Story/TADialogueWidget.h
// 对话主 UI（WBP_Dialogue 的 C++ 基类）
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Story/TADialogueTypes.h"
#include "TADialogueWidget.generated.h"

class UTADialogueController;
class UTADialogueSubsystem;
class UTADialoguePortraitLayerWidget;
class UTAPortraitWidget;
class UTADialogueChoiceButton;
class UTADialogueHistoryWidget;
class UTAActionPromptWidget;
class UTALocalizeSubsystem;
class UTAInputIconSubsystem;
class UTextBlock;
class UButton;
class UImage;
class UHorizontalBox;
class UInputAction;
class UInputMappingContext;
class UWidget;
class UCanvasPanel;
class UVerticalBox;
class UPanelWidget;

/**
 * 控件命名约定（WBP_Dialogue，布局全部在编辑器里摆，C++ 只绑定逻辑）：
 * 必需：
 *   Text_Name          UTextBlock   角色名字框
 *   Text_Dialogue      UTextBlock   对话框
 *   Button_Continue    UButton      继续按钮
 *   Button_History     UButton      历史按钮
 *   Box_Choices        UVerticalBox 选项列表容器
 * 立绘由独立 WBP_DialoguePortraitLayer 承载，显示在主对话 UI 后方。
 * 可选：
 *   Panel_Choices      UPanelWidget 分支面板整体（显隐用；缺省时用 Box_Choices 自己）
 *   独立历史面板 WBP_DialogueHistory 由玩家角色蓝图配置并在顶层创建。
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

	/** 由子系统调用：绑定会话和独立立绘层 */
	void Setup(UTADialogueSubsystem* InSubsystem, UTADialogueController* InController, UTADialoguePortraitLayerWidget* InPortraitLayer = nullptr);

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
	void EnsureBindings();
	void RefreshLine();
	void RefreshChoices();
	void RefreshPortraits();
	void RefreshHistory();
	void RefreshIcons();
	void RefreshPromptLabels();
	void BuildActionPromptBar();
	void RefreshActionPromptBar();
	void UpdateChoiceHighlights();
	void UpdateChoiceSelectionFromMouse();
	void EnsureChoiceInputActions();
	void OnChoicePreviousPressed();
	void OnChoiceNextPressed();
	void OnChoiceConfirmPressed();
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
	UFUNCTION()
	void OnChoiceFocused(int32 Index);
	UFUNCTION()
	void OnActionPromptClicked(UTAActionPromptWidget* Prompt);

	UFUNCTION()
	void CloseHistoryOverlay();

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

	/** Enhanced Input actions for choice navigation and confirmation. Missing actions are created at runtime. */
	UPROPERTY(EditAnywhere, Category = "Dialogue|Input|Choices")
	TObjectPtr<UInputAction> ChoicePreviousAction;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Input|Choices")
	TObjectPtr<UInputAction> ChoiceNextAction;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Input|Choices")
	TObjectPtr<UInputAction> ChoiceConfirmAction;

	/** Optional styled prompt WBP; falls back to the native action-prompt widget. */
	UPROPERTY(EditAnywhere, Category = "Dialogue|Input|Choices")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	/** 对话期间激活的映射上下文（高优先级屏蔽移动等） */
	UPROPERTY(EditAnywhere, Category = "Dialogue|Input")
	TObjectPtr<UInputMappingContext> DialogueMappingContext;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Input")
	int32 DialogueMappingPriority = 10;

	// ==================== 绑定控件 ====================

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Dialogue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Continue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_History;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> Box_Choices;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Choices;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LegacyEmbeddedHistoryWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ContinueIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_HistoryIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ContinueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HistoryText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	// ==================== 动态创建 ====================

	UPROPERTY()
	TArray<TObjectPtr<UTAPortraitWidget>> PortraitWidgets;

	/** Separate WBP rendered beneath this dialogue widget. */
	UPROPERTY(Transient)
	TObjectPtr<UTADialoguePortraitLayerWidget> PortraitLayer;

	UPROPERTY(Transient)
	TObjectPtr<UTADialogueHistoryWidget> ActiveHistoryWidget;

	UPROPERTY()
	TArray<TObjectPtr<UTADialogueChoiceButton>> ChoiceWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAActionPromptWidget>> ChoicePromptWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAActionPromptWidget>> BasePromptWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAActionPromptWidget>> ActionPromptWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeChoiceMappingContext;

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
	/** 防止同一物理按键同时映射到 AdvanceAction 和 HistoryAction 时打开历史。 */
	uint64 AdvancePressedFrame = MAX_uint64;
	/** 防止同一物理按键同时映射到 AdvanceAction 和 ChoiceConfirmAction 时重复选择。 */
	uint64 ChoiceConfirmPressedFrame = MAX_uint64;
	FVector2D LastChoiceMousePosition = FVector2D::ZeroVector;
	bool bHasLastChoiceMousePosition = false;
	bool bRefreshIconsOnNextTick = false;
	ESlateVisibility ContinueButtonVisibilityBeforeHistory = ESlateVisibility::Visible;
	ESlateVisibility HistoryButtonVisibilityBeforeHistory = ESlateVisibility::Visible;
	bool bContinueButtonWasEnabledBeforeHistory = true;

	bool bHistoryOpen = false;
	bool bPreviewMode = false;
	bool bMappingPushed = false;
};
