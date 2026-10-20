#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "UI/TASelectableMenuOptionWidget.h"
#include "Inventory/TAItemTypes.h"
#include "TAItemActionMenuWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class UVerticalBox;
class UInputAction;
class UTAItemDefinition;
class UTAItemActionMenuWidget;

/** 物品交互菜单里的一个选项 */
USTRUCT()
struct THE_AWAKENING_API FTAItemActionOption
{
	GENERATED_BODY()

	FName ActionId;
	FString LabelTextId;
	/** 暂不可用（尚未实现、条件不满足）的选项显示为置灰，不能执行 */
	bool bEnabled = true;
};

/**
 * 物品交互菜单的一行（WBP_ItemActionOption 的父类）。
 * 控件命名：Button_Option（UButton）、Text_Option（UTextBlock，放在按钮里）；可选 Border_Highlight（选中时显示）。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAItemActionOptionWidget : public UTASelectableMenuOptionWidget
{
	GENERATED_BODY()

public:
	void Setup(UTAItemActionMenuWidget* InMenu, int32 InIndex, const FText& Label, bool bEnabled);
	void SetHighlighted(bool bHighlighted);
	bool IsOptionEnabled() const { return bOptionEnabled; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Option;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Option;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Highlight;

private:
	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UTAItemActionMenuWidget> Menu;
	int32 Index = INDEX_NONE;
	bool bOptionEnabled = true;
};

/**
 * 物品交互菜单（WBP_ItemActionMenu 的父类）：背包里光标停在物品上，按 IA_ItemActions（IMC_UI 中配置）打开。
 * 控件命名：
 *   Panel_Menu（任意 UWidget，放在根 Canvas 上，自动大小）：菜单本体，打开时移到光标旁
 *   VerticalBox_Options（UVerticalBox，在 Panel_Menu 里）：选项由 C++ 用 OptionWidgetClass 生成
 *   根 Canvas 上再铺一层全屏、可点击的透明控件（例如 Image_ClickCatcher），用来接住空白处的点击
 *
 * 任何时候都有一个选中项，打开时固定选中第一项：
 *   上下选择（IA_ChoicePrevious / IA_ChoiceNext）换选中项；光标（鼠标或手柄虚拟光标）移到某项上即选中该项；
 *   确定键（IA_Inventory_Confirm、IA_ChoiceConfirm）执行选中项；鼠标点击执行被点的那一项。
 * 点击菜单以外的地方关闭菜单，这次点击被菜单吃掉，不会点到后面的背包。返回键关闭菜单。
 * 置灰的选项可以被选中，但不能执行。优先级高于背包、低于日记本；时停沿用背包的。
 *
 * 选项（策划文档）：普通物品「检视」+「丢弃」，可堆叠时「丢弃」换成「部分丢弃」并多一个「拆分堆叠」；
 * 日记本「查看内容」；其他剧情物品「检视」。剧情物品不能丢弃。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAItemActionMenuWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPriority = 210;

	static const FName ActionViewContent;
	static const FName ActionInspect;
	static const FName ActionDrop;
	static const FName ActionPartialDrop;
	static const FName ActionSplit;

	/** 物品（格子里的一叠）可用的交互选项；为空表示不弹菜单 */
	static TArray<FTAItemActionOption> GetOptionsForSlot(const FTAInventorySlot& Slot, bool bHasEmptySlot);

	/**
	 * 在光标位置打开菜单。没有可用选项时返回 nullptr。
	 * @param OnChosen 选中某项后调用（菜单已关闭），参数为选项 ID
	 */
	static UTAItemActionMenuWidget* Open(APlayerController* PC, TSubclassOf<UTAItemActionMenuWidget> WidgetClass,
		TArray<FTAItemActionOption> Options, TFunction<void(FName ActionId)> OnChosen);

	void Close();
	void ChooseOption(int32 OptionIndex);
	void SelectOption(int32 OptionIndex);

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override;
	bool AllowsPlayerInput(ETAInputCapability Capability) const;

	virtual void RemoveFromParent() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Action Menu")
	TSubclassOf<UTAItemActionOptionWidget> OptionWidgetClass;

	/** 执行选中项。为空时用背包的 /Game/Input/Actions/Inventory/IA_Inventory_Confirm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Action Menu|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	/** 同样执行选中项（已有 UI 的确认）。为空时用 /Game/Input/Actions/IA_ChoiceConfirm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Action Menu|Input")
	TObjectPtr<UInputAction> ChoiceConfirmAction;

	/** 上一项 / 下一项（已有 UI 的上下选择）。为空时用 /Game/Input/Actions/IA_ChoicePrevious、IA_ChoiceNext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Action Menu|Input")
	TObjectPtr<UInputAction> PreviousAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Action Menu|Input")
	TObjectPtr<UInputAction> NextAction;

	/** 菜单左上角相对光标的偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Action Menu")
	FVector2D CursorOffset = FVector2D(16.f, 8.f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	/** 点在菜单以外：关闭并吃掉这次点击（菜单内的点击先被选项按钮处理） */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Menu;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox_Options;

private:
	void BuildOptions();
	void PlaceAtCursor();
	void ReleaseInput();

	UFUNCTION()
	void HandleOptionHovered(UTASelectableMenuOptionWidget* Option);

	TArray<FTAItemActionOption> Options;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAItemActionOptionWidget>> OptionWidgets;

	TFunction<void(FName)> OnChosen;
	int32 SelectedIndex = 0;

	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
};
