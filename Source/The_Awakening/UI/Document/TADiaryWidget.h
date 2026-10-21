#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "TADiaryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UHorizontalBox;
class UInputAction;
class UTADocumentDefinition;
class UTAActionPromptWidget;
class UTADocumentReaderWidget;
class ATAPreviewStage;
class ATADiaryPreviewStage;
enum class ETADiaryMotion : uint8;

/**
 * 日记本「查看内容」界面（WBP_Diary 的父类）。从背包剧情格里的日记本打开，关闭回到背包。
 *
 * 控件命名约定：
 *   Panel_Book（任意 UWidget）：3D 书本及夹页共用画面的容器，承载 UI 飞入/飞出变换
 *   Image_Book（UImage）：3D 日记展示台的透明画面
 *   Panel_Page（任意 UWidget）：标题容器，书本动作期间隐藏
 *   Text_DocumentTitle（UTextBlock）：文本标题
 *   Text_PageNumber（UTextBlock）：页码“当前 / 总数”
 *   Text_Empty（UTextBlock）：一份都没收集时显示
 *   HorizontalBox_Controls（UHorizontalBox）：底部操作栏，C++ 生成“上一页 / 下一页 / 查看 / 返回”（可点击）
 *
 * 规则（策划文档）：按收集顺序一页一份；0 份是空书，0 / 1 份不能翻页；退出时保存页码，下次打开回到这一页。
 * 3D 书本由专用展示台驱动，UI 保留从剧情格放大/缩回的过渡。
 * 动画没播完之前返回键、翻页、查看都无效。时停沿用背包的；手柄时隐藏光标。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADiaryWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPriority = 220;

	/**
	 * 打开日记本。
	 * @param SourceScreenPosition 剧情格中心（视口坐标，Slate 单位）；书从这里放大、关闭时缩回这里。为空时从屏幕中心
	 * @param OpenPage 打开到指定页（快速查看用），INDEX_NONE 时用上次保存的页码
	 */
	static UTADiaryWidget* Open(APlayerController* PC, TSubclassOf<UTADiaryWidget> WidgetClass,
		TOptional<FVector2D> SourceScreenPosition, int32 OpenPage = INDEX_NONE,
		bool bSkipOpenAnimation = false, bool bOpenDocumentImmediately = false,
		TOptional<FVector2D> SourceScreenSize = {}, UWidget* SourceIcon = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Diary")
	void RequestClose();

	/** 翻页：-1 上一页，+1 下一页。0 / 1 份或动画中无效。 */
	UFUNCTION(BlueprintCallable, Category = "Diary")
	void TurnPage(int32 Direction);

	/** 长按翻页键跳到首/末页所需时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Input", meta = (ClampMin = "0.2"))
	float PageHoldDuration = 0.65f;

	/** 打开当前页文本的预览 */
	UFUNCTION(BlueprintCallable, Category = "Diary")
	void OpenCurrentDocument();

	UFUNCTION(BlueprintPure, Category = "Diary")
	int32 GetCurrentPage() const { return CurrentPage; }

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool UsesEnhancedUIActions() const override { return true; }
	virtual void ExecutePlayerAction(const UInputAction* Action, ETriggerEvent Event, const FInputActionInstance& Instance) override;
	virtual bool HandleMenuBackRequested() override;
	virtual bool AllowsVirtualCursor() const override { return false; }
	virtual bool ShouldShowPlayerCursor() const override { return !bGamepadDevice; }
	bool AllowsPlayerInput(ETAInputCapability Capability) const;

	virtual void RemoveFromParent() override;

	// ==================== 输入（动作在 IMC_UI 中配置按键） ====================

	/** 为空时用 /Game/Input/Actions/UI/IA_DiaryPreviousPage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Input")
	TObjectPtr<UInputAction> PreviousPageAction;

	/** 为空时用 /Game/Input/Actions/UI/IA_DiaryNextPage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Input")
	TObjectPtr<UInputAction> NextPageAction;

	/** 查看当前页文本。为空时用背包的确认 /Game/Input/Actions/Inventory/IA_Inventory_Confirm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Input")
	TObjectPtr<UInputAction> ReadAction;

	/** 返回提示显示的动作（实际返回走 IA_UIBack）。为空时用 /Game/Input/Actions/IA_UIBack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Input")
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	/** 打开文本预览使用的界面类，为空时用项目设置里的 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary")
	TSubclassOf<UTADocumentReaderWidget> ReaderWidgetClass;

	// ==================== 参数 ====================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Book", meta = (ClampMin = "0.01"))
	float BookPlaybackRate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Book")
	float BookCameraPitch = -65.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Book", meta = (ClampMin = "1"))
	float BookFramingPadding = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Book")
	FTransform BookTransform = FTransform(FRotator(0.f, 90.f, 0.f));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Paper", meta = (ClampMin = "1"))
	float DocumentSize = 14.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Paper")
	FVector RightDocumentPosition = FVector(7.7f, 0.f, 1.25f);

	/** 书在剧情格位置时的缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Animation", meta = (ClampMin = "0.01", ClampMax = "1"))
	float CollapsedScale = 0.08f;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString PreviousPageTextId = TEXT("UI_Diary_PreviousPage");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString NextPageTextId = TEXT("UI_Diary_NextPage");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString FirstPageHoldTextId = TEXT("UI_Diary_FirstPageHold");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString LastPageHoldTextId = TEXT("UI_Diary_LastPageHold");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString ReadTextId = TEXT("UI_Diary_Read");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString BackTextId = TEXT("UI_Document_Back");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString EmptyTextId = TEXT("UI_Diary_Empty");

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Book;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Book;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Page;


	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_DocumentTitle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PageNumber;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	/** UI travel around the independently animated 3D book. */
	void ApplyOpenCloseVisual(float Alpha);

private:
	friend class FTADiaryWidgetIntegrationTest;
	friend class FTADiaryEnhancedInputTest;
	friend class UTADocumentUICommandlet;
	enum class EPhase : uint8 { Opening, Open, Flipping, Reading, Closing };

	void LoadDefaultAssets();
	void BuildControls();
	void ShowPage(int32 PageIndex);
	void RefreshControls();
	void TurnToPage(int32 Page);
	void UpdatePageHold(float DeltaSeconds, bool bHeld, bool bOwnsInput);
	void AcquireInput();
	void ReleaseInput();
	void ReleasePreviewStages();
	void RefreshPageVisibility();
	UFUNCTION()
	void HandleBookMotionFinished(ETADiaryMotion CompletedMotion);
	FText Localize(const FString& TextId) const;
	int32 GetPageCount() const;


	UFUNCTION()
	void HandlePromptClicked(UTAActionPromptWidget* Prompt);

	UFUNCTION()
	void HandleInputPresentationChanged();

	UFUNCTION()
	void HandleLanguageChanged();

	UPROPERTY(Transient)
	TObjectPtr<ATADiaryPreviewStage> BookStage;
	UPROPERTY(Transient)
	TObjectPtr<UTADocumentReaderWidget> ActiveReader;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> PreviousPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> NextPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> FirstPageHoldPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> LastPageHoldPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> ReadPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> BackPrompt;

	TOptional<FVector2D> SourcePosition;
	TOptional<FVector2D> SourceSize;
	FBox2D ClosedBookBounds = FBox2D(ForceInit);
	TWeakObjectPtr<UWidget> SourceIconWidget;
	void UpdateSourceIconGeometry();
	bool bPageActionHeld = false;
	int32 HeldPageDirection = 0;
	int32 QueuedEndpoint = 0;
	float PageHeldTime = 0.f;
	bool bPageHoldTriggered = false;
	bool bPendingOpenFromIcon = false;
	bool bPendingQuickRead = false;
	/** 快速查看：跳过入场动画，打开后直接进入当前页文本的预览 */
	bool bSkipOpenAnimation = false;
	bool bOpenDocumentImmediately = false;
	int32 RequestedPage = INDEX_NONE;
	int32 CurrentPage = 0;
	int32 PendingPage = 0;

	EPhase Phase = EPhase::Opening;
	bool bGamepadDevice = false;

	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
};
