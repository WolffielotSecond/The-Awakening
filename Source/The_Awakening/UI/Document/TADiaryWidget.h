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

/**
 * 日记本「查看内容」界面（WBP_Diary 的父类）。从背包剧情格里的日记本打开，关闭回到背包。
 *
 * 控件命名约定：
 *   Panel_Book（任意 UWidget）：整本书，打开 / 关闭 / 翻页动画作用在它上面
 *   Panel_Page（任意 UWidget）：右书页上夹着的文本（翻页时淡出淡入）
 *   Button_Document（UButton）：点击打开文本预览；里面放 Image_Document（UImage，文本的 3D 模型画面）
 *   Text_DocumentTitle（UTextBlock）：文本标题
 *   Text_PageNumber（UTextBlock）：页码“当前 / 总数”
 *   Text_Empty（UTextBlock）：一份都没收集时显示
 *   HorizontalBox_Controls（UHorizontalBox）：底部操作栏，C++ 生成“上一页 / 下一页 / 查看 / 返回”（可点击）
 *
 * 规则（策划文档）：按收集顺序一页一份；0 份是空书，0 / 1 份不能翻页；退出时保存页码，下次打开回到这一页。
 * 打开时从剧情格位置放大淡入，关闭时缩回剧情格位置（UMG 2D 占位，以后换 3D 书本只改 PlayOpenClose / PlayPageFlip）。
 * 动画没播完之前返回键、翻页、查看都无效。时停沿用背包的；手柄时隐藏光标。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADiaryWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPriority = 220;
	/** 日记本页面上文本模型使用的预览台槽位 */
	static constexpr int32 PreviewStageSlot = 1;

	/**
	 * 打开日记本。
	 * @param SourceScreenPosition 剧情格中心（视口坐标，Slate 单位）；书从这里放大、关闭时缩回这里。为空时从屏幕中心
	 * @param OpenPage 打开到指定页（快速查看用），INDEX_NONE 时用上次保存的页码
	 */
	static UTADiaryWidget* Open(APlayerController* PC, TSubclassOf<UTADiaryWidget> WidgetClass,
		TOptional<FVector2D> SourceScreenPosition, int32 OpenPage = INDEX_NONE,
		bool bSkipOpenAnimation = false, bool bOpenDocumentImmediately = false);

	UFUNCTION(BlueprintCallable, Category = "Diary")
	void RequestClose();

	/** 翻页：-1 上一页，+1 下一页。0 / 1 份或动画中无效。 */
	UFUNCTION(BlueprintCallable, Category = "Diary")
	void TurnPage(int32 Direction);

	/** 打开当前页文本的预览 */
	UFUNCTION(BlueprintCallable, Category = "Diary")
	void OpenCurrentDocument();

	UFUNCTION(BlueprintPure, Category = "Diary")
	int32 GetCurrentPage() const { return CurrentPage; }

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Animation", meta = (ClampMin = "0", Units = "s"))
	float OpenDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Animation", meta = (ClampMin = "0", Units = "s"))
	float CloseDuration = 0.4f;

	/** 翻页过渡总时长（前一半淡出旧页，后一半淡入新页） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Animation", meta = (ClampMin = "0", Units = "s"))
	float PageFlipDuration = 0.3f;

	/** 书在剧情格位置时的缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Animation", meta = (ClampMin = "0.01", ClampMax = "1"))
	float CollapsedScale = 0.08f;

	/** 页面文本模型画面分辨率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary")
	FIntPoint PageRenderSize = FIntPoint(768, 768);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString PreviousPageTextId = TEXT("UI_Diary_PreviousPage");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Text")
	FString NextPageTextId = TEXT("UI_Diary_NextPage");

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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Book;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Page;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Document;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Document;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_DocumentTitle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PageNumber;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	/** 以后换 3D 书本时替换这两个：当前是 UMG 2D 过渡 */
	void ApplyOpenCloseVisual(float Alpha);
	void ApplyPageFlipVisual(float Progress);

private:
	enum class EPhase : uint8 { Opening, Open, Flipping, Reading, Closing };

	void LoadDefaultAssets();
	void BuildControls();
	void ShowPage(int32 PageIndex);
	void RefreshControls();
	void AcquireInput();
	void ReleaseInput();
	FText Localize(const FString& TextId) const;
	int32 GetPageCount() const;

	UFUNCTION()
	void HandleDocumentClicked();

	UFUNCTION()
	void HandlePromptClicked(UTAActionPromptWidget* Prompt);

	UFUNCTION()
	void HandleInputPresentationChanged();

	UFUNCTION()
	void HandleLanguageChanged();

	UPROPERTY(Transient)
	TObjectPtr<ATAPreviewStage> PreviewStage;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> PreviousPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> NextPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> ReadPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UTAActionPromptWidget> BackPrompt;

	TOptional<FVector2D> SourcePosition;
	/** 快速查看：跳过入场动画，打开后直接进入当前页文本的预览 */
	bool bSkipOpenAnimation = false;
	bool bOpenDocumentImmediately = false;
	int32 RequestedPage = INDEX_NONE;
	int32 CurrentPage = 0;
	int32 PendingPage = 0;

	EPhase Phase = EPhase::Opening;
	float PhaseTime = 0.f;
	bool bPageSwapped = false;
	bool bGamepadDevice = false;

	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
};
