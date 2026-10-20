#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "TAQuantityDialogWidget.generated.h"

class UButton;
class UTextBlock;
class UHorizontalBox;
class UInputAction;
class UTAActionPromptWidget;

/**
 * 数量选择小窗口（WBP_QuantityDialog 的父类）：拆分堆叠、部分丢弃用。策划文档里的 “< 10 >” 加确定 / 取消。
 *
 * 控件命名：
 *   Panel_Dialog（任意 UWidget）：窗口本体；点在它以外的地方会被吃掉（不穿透到背包），但不关闭窗口
 *   Text_Title（UTextBlock）：操作名（拆分堆叠 / 部分丢弃）
 *   Button_Decrease、Button_Increase（UButton）：“<” “>”
 *   Text_Quantity（UTextBlock）：当前数量
 *   Button_Confirm、Button_Cancel（UButton）；可选 Text_Confirm、Text_Cancel（按钮文字，C++ 填本地化文本）
 *   可选 HorizontalBox_Controls：底部按键提示（减少 / 增加 / 确定 / 取消），可点击
 *
 * 操作沿用背包：光标常显，键鼠点按钮；手柄可用虚拟光标点按钮，
 * 也可以直接按 IA_QuantityDecrease / IA_QuantityIncrease（IMC_UI 中配置）调数量，确定键确认，返回键取消。
 * 数量范围 [Min, Max]，打开时为 Min。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAQuantityDialogWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPriority = 215;

	/**
	 * 打开窗口。
	 * @param OnConfirm 确定后调用（窗口已关闭），参数为选定数量；取消时不调用
	 */
	static UTAQuantityDialogWidget* Open(APlayerController* PC, TSubclassOf<UTAQuantityDialogWidget> WidgetClass,
		const FString& TitleTextId, int32 Min, int32 Max, TFunction<void(int32 Quantity)> OnConfirm);

	void ChangeQuantity(int32 Delta);
	void Confirm();
	void Cancel();

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override;
	bool AllowsPlayerInput(ETAInputCapability Capability) const;

	virtual void RemoveFromParent() override;

	/** 为空时用 /Game/Input/Actions/UI/IA_QuantityDecrease */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quantity Dialog|Input")
	TObjectPtr<UInputAction> DecreaseAction;

	/** 为空时用 /Game/Input/Actions/UI/IA_QuantityIncrease */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quantity Dialog|Input")
	TObjectPtr<UInputAction> IncreaseAction;

	/** 为空时用背包的 /Game/Input/Actions/Inventory/IA_Inventory_Confirm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quantity Dialog|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	/** 同样确认（已有 UI 的确认）。为空时用 /Game/Input/Actions/IA_ChoiceConfirm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quantity Dialog|Input")
	TObjectPtr<UInputAction> ChoiceConfirmAction;

	/** 取消提示显示的动作（实际取消走 IA_UIBack）。为空时用 /Game/Input/Actions/IA_UIBack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quantity Dialog|Input")
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quantity Dialog|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Dialog;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Quantity;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Decrease;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Increase;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Cancel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Confirm;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Cancel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

private:
	void Refresh();
	void BuildControls();
	void ReleaseInput();
	FText Localize(const FString& TextId) const;

	UFUNCTION() void HandleDecreaseClicked();
	UFUNCTION() void HandleIncreaseClicked();
	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();
	UFUNCTION() void HandlePromptClicked(UTAActionPromptWidget* Prompt);

	FString TitleTextId;
	int32 MinQuantity = 1;
	int32 MaxQuantity = 1;
	int32 Quantity = 1;
	TFunction<void(int32)> OnConfirm;

	UPROPERTY(Transient) TObjectPtr<UTAActionPromptWidget> DecreasePrompt;
	UPROPERTY(Transient) TObjectPtr<UTAActionPromptWidget> IncreasePrompt;
	UPROPERTY(Transient) TObjectPtr<UTAActionPromptWidget> ConfirmPrompt;
	UPROPERTY(Transient) TObjectPtr<UTAActionPromptWidget> CancelPrompt;

	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
};
