#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "TADocumentReaderWidget.generated.h"

class UTextBlock;
class URichTextBlock;
class UScrollBox;
class UImage;
class UBackgroundBlur;
class UHorizontalBox;
class UInputAction;
class UTADocumentDefinition;
class UTAActionPromptWidget;
class ATAPreviewStage;

/**
 * 文本预览界面（WBP_DocumentReader 的 C++ 父类）。所有文本预览共用：场景拿起、日记本、拾取提示快速查看。
 *
 * 默认显示文档的 3D 模型，可拖动（鼠标）或用 RotateAction（手柄）旋转；
 * “显示转译”使背景变糊变暗，并在屏幕中央显示无底框的标题 + 正文，再按一次收起。
 *
 * 控件命名约定：
 *   Image_Model（UImage）：3D 模型画面，建议放在 ScaleBox 里保持比例；在它上面按住鼠标拖动旋转
 *   Panel_Translation（任意 UWidget）：转译面板，默认隐藏，包住下面三个
 *     Text_Title（UTextBlock）：标题，不随正文滚动
 *     ScrollBox_Body（UScrollBox）+ RichText_Body（URichTextBlock，在 ScrollBox 内）：正文，<img id="key"/> 插图
 *   HorizontalBox_Controls（UHorizontalBox）：底部操作栏，C++ 按顺序生成“显示转译 / 旋转 / 返回”的按键提示，
 *     “显示转译”“返回”可以点击（与背包操作栏相同的 WBP_ActionPrompt）；“旋转”只在手柄下显示（键鼠用鼠标拖动）
 * 转译背景位于模型之上、文字和底部操作栏之下；强度在 Translation 参数中调整。
 *
 * 行为：
 * - 打开 / 关闭时整体淡入淡出（真实时间），动画没播完之前返回键和按钮都无效。
 * - 转译面板显示时，正文用 ScrollAction（↑↓、右摇杆）和滚轮平滑滚动；隐藏时右摇杆改为旋转模型。
 * - bFreezeTime：从场景直接打开时自己申请时停；从日记本打开时沿用日记本的时停。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADocumentReaderWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	/** 场景直接打开时的输入优先级（与背包同级）；从日记本打开时由日记本传入更高的优先级 */
	static constexpr int32 StandaloneInputPriority = 200;
	/** 文本预览独占的预览台槽位 */
	static constexpr int32 PreviewStageSlot = 3;

	/**
	 * 创建并显示文本预览。WidgetClass 为空时用项目设置里的类。
	 * @param OnClosed 关闭动画结束（或被强制关闭）后调用一次；bForced 为 true 表示被强制关闭（死亡回滚等）
	 */
	static UTADocumentReaderWidget* Open(APlayerController* PC, TSubclassOf<UTADocumentReaderWidget> WidgetClass,
		UTADocumentDefinition* Document, int32 InputPriority, bool bFreezeTime, TFunction<void(bool bForced)> OnClosed,
		ATAPreviewStage* SharedStage = nullptr, UImage* DiaryImage = nullptr);

	/** 开始关闭（播放关闭动画）。打开动画没播完时无效。 */
	UFUNCTION(BlueprintCallable, Category = "Document Reader")
	void RequestClose();

	/** 立即关闭，不播动画（死亡回滚、Actor 销毁时用） */
	void ForceClose();

	/** 显示 / 收起转译面板。动画没播完时无效。 */
	UFUNCTION(BlueprintCallable, Category = "Document Reader")
	void ToggleTranslation();

	UFUNCTION(BlueprintPure, Category = "Document Reader")
	bool IsTranslationVisible() const { return bTranslationVisible; }

	UFUNCTION(BlueprintPure, Category = "Document Reader")
	UTADocumentDefinition* GetDocument() const { return Document; }

	UFUNCTION(BlueprintPure, Category = "Document Reader")
	bool IsTransitioning() const { return Phase != EPhase::Open; }

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override;
	virtual bool AllowsVirtualCursor() const override { return false; }
	/** 与暂停菜单一致：手柄时隐藏光标，切回键鼠时恢复 */
	virtual bool ShouldShowPlayerCursor() const override { return !bGamepadDevice; }
	bool AllowsPlayerInput(ETAInputCapability Capability) const;

	virtual void RemoveFromParent() override;

	// ==================== 输入（动作在 IMC_UI 中配置按键） ====================

	/** 显示 / 收起转译。为空时用 /Game/Input/Actions/UI/IA_DocumentTranslate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Input")
	TObjectPtr<UInputAction> TranslateAction;

	/** 旋转模型（Axis2D，X 绕竖直轴、Y 俯仰）。为空时用 /Game/Input/Actions/UI/IA_DocumentRotate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Input")
	TObjectPtr<UInputAction> RotateAction;

	/** 正文滚动（Axis1D，正值向下）。为空时用 /Game/Input/Actions/UI/IA_ReaderScroll */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Input")
	TObjectPtr<UInputAction> ScrollAction;

	/** 返回提示显示的动作（实际返回走 IA_UIBack 的统一返回）。为空时用 /Game/Input/Actions/IA_UIBack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Input")
	TObjectPtr<UInputAction> BackAction;

	/** 操作栏按键提示控件。为空时用 /Game/UI/WBP_ActionPrompt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	// ==================== 参数（WBP 里调） ====================

	/** 鼠标拖动旋转灵敏度（度 / 像素） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Model", meta = (ClampMin = "0"))
	float DragRotateSensitivity = 0.4f;

	/** 摇杆满偏时的旋转速度（度 / 秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Model", meta = (ClampMin = "0"))
	float StickRotateSpeed = 180.f;

	/** 模型画面分辨率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Model")
	FIntPoint ModelRenderSize = FIntPoint(1024, 1024);

	/** 方向键 / 摇杆满偏时的滚动速度（像素 / 秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Scroll", meta = (ClampMin = "0"))
	float HeldScrollSpeed = 900.f;

	/** 滚轮每格滚动的距离（像素） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Scroll", meta = (ClampMin = "0"))
	float WheelScrollStep = 120.f;

	/** 平滑程度：越大越快贴近目标位置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Scroll", meta = (ClampMin = "1"))
	float ScrollSmoothing = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Animation", meta = (ClampMin = "0", Units = "s"))
	float OpenDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Animation", meta = (ClampMin = "0", Units = "s"))
	float CloseDuration = 0.2f;

	/** 转译面板淡入 / 淡出时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Animation", meta = (ClampMin = "0", Units = "s"))
	float TranslationFadeDuration = 0.15f;

	/** 转译时覆盖模型和界面背景的虚化强度，不影响转译文字和底部按键提示。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Translation", meta = (ClampMin = "0", UIMax = "30"))
	float TranslationBlurStrength = 12.f;

	/** 转译暗幕的不透明度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Translation", meta = (ClampMin = "0", ClampMax = "1"))
	float TranslationDimOpacity = 0.6f;

	/** 正文字号。正文没有配置文字样式表时，沿用标题的字体并使用这个字号。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Text", meta = (ClampMin = "8"))
	int32 BodyFontSize = 22;

	/** 操作栏文字的本地化 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Text")
	FString ShowTranslationTextId = TEXT("UI_Document_ShowTranslation");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Text")
	FString HideTranslationTextId = TEXT("UI_Document_HideTranslation");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Text")
	FString RotateTextId = TEXT("UI_Document_Rotate");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document Reader|Text")
	FString BackTextId = TEXT("UI_Document_Back");

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Model;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Translation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Body;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> RichText_Body;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

private:
	friend class FTAPaperReaderTransitionTest;
	friend class FTAReaderTranslationLayoutTest;
	enum class EPhase : uint8 { Opening, Open, Closing };

	void LoadDefaultAssets();
	void SetupModel();
	void BuildControls();
	void SetupTranslationPresentation();
	void TickTranslation(float DeltaTime);
	void RefreshText();
	void AcquireInputAndFreeze();
	void ReleaseInputAndFreeze();
	void FinishClose(bool bForced);
	void ClampScrollTarget();
	void TickScroll(float DeltaTime);
	void TickPaperTransition(float DeltaTime);
	void RestoreDiaryPresentation();
	void ApplyPaperTravel(float Progress);

	UFUNCTION()
	void HandleLanguageChanged();

	UFUNCTION()
	void HandlePromptClicked(UTAActionPromptWidget* Prompt);

	/**
	 * 输入设备或键位变化时调用：与背包一致，只用于手柄的提示（旋转）在键鼠下收起；
	 * 与暂停菜单一致，手柄时隐藏光标（通知控制器重新同步光标显示）
	 */
	UFUNCTION()
	void RefreshPromptVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UTADocumentDefinition> Document;

	UPROPERTY(Transient)
	TObjectPtr<ATAPreviewStage> PreviewStage;

	TWeakObjectPtr<UImage> SourceDiaryImage;
	bool bBorrowedStage = false;
	bool bPaperTransition = false;
	bool bPaperLayoutReady = false;
	bool bDiaryPresentationRestored = false;
	FVector2D DiaryAbsoluteCenter = FVector2D::ZeroVector;
	FVector2D DiaryAbsoluteSize = FVector2D::ZeroVector;
	FGeometry ReaderTargetGeometry;
	FTransform DiaryModelTransform = FTransform::Identity;
	FTransform ReaderModelTransform = FTransform::Identity;
	FQuat ClosingManualRotation = FQuat::Identity;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> TranslatePrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> BackPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> RotatePrompt;

	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> TranslationBlur;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TranslationDim;

	int32 InputPriority = StandaloneInputPriority;
	bool bFreezeTime = false;
	bool bFreezeRequested = false;
	/** 走完关闭动画为 true；其他方式移除（强制关闭、外部移除）回调时 bForced = true */
	bool bClosedNormally = false;
	TFunction<void(bool)> OnClosed;

	EPhase Phase = EPhase::Opening;
	float PhaseTime = 0.f;

	bool bGamepadDevice = false;

	bool bTranslationVisible = false;
	/** 转译面板当前透明度（0 隐藏，1 显示），向 bTranslationVisible 过渡 */
	float TranslationAlpha = 0.f;

	bool bDraggingModel = false;
	FVector2D LastDragPosition = FVector2D::ZeroVector;

	float ScrollTarget = 0.f;
	float LastAppliedScroll = 0.f;

	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
};
