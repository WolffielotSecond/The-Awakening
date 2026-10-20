#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAInputRouter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "TAItemInspectWidget.generated.h"

class UImage;
class UTextBlock;
class UHorizontalBox;
class UInputAction;
class UTAItemDefinition;
class UTAActionPromptWidget;
class ATAPreviewStage;

/**
 * 物品「检视」界面（WBP_ItemInspect 的父类）：从背包物品的交互菜单打开，返回回到背包。
 * 策划文档：背包变暗变模糊，3D 模型在屏幕中央，名称和介绍在模型下方，可拖动或用右摇杆旋转，ESC / B / ○ 退出。
 *
 * 控件命名：
 *   Image_Model（UImage）：3D 模型画面，建议放在 ScaleBox 里保持比例；在它上面按住鼠标拖动旋转
 *   Text_Name（UTextBlock）、Text_Description（UTextBlock）：名称、介绍
 *   HorizontalBox_Controls（UHorizontalBox）：底部操作栏，C++ 生成“旋转 / 返回”，旋转只在手柄下显示，返回可点击
 *
 * 旋转与文本预览相同：固定轴、可无限翻转；鼠标拖动，手柄用 RotateAction。
 * 打开 / 关闭淡入淡出（真实时间），动画没播完之前返回无效。时停沿用背包的；手柄时隐藏光标。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAItemInspectWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPriority = 220;
	static constexpr int32 PreviewStageSlot = 4;

	static UTAItemInspectWidget* Open(APlayerController* PC, TSubclassOf<UTAItemInspectWidget> WidgetClass, UTAItemDefinition* Item);

	UFUNCTION(BlueprintCallable, Category = "Item Inspect")
	void RequestClose();

	// ITAPlayerInputReceiver
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override { return {}; }
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override {}
	virtual bool HandleMenuBackRequested() override;
	virtual bool AllowsVirtualCursor() const override { return false; }
	virtual bool ShouldShowPlayerCursor() const override { return !bGamepadDevice; }
	bool AllowsPlayerInput(ETAInputCapability Capability) const;

	virtual void RemoveFromParent() override;

	/** 旋转模型（Axis2D）。为空时用 /Game/Input/Actions/UI/IA_DocumentRotate（与文本预览共用） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Input")
	TObjectPtr<UInputAction> RotateAction;

	/** 返回提示显示的动作（实际返回走 IA_UIBack）。为空时用 /Game/Input/Actions/IA_UIBack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Input")
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Model", meta = (ClampMin = "0"))
	float DragRotateSensitivity = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Model", meta = (ClampMin = "0"))
	float StickRotateSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Model")
	FIntPoint ModelRenderSize = FIntPoint(1024, 1024);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Animation", meta = (ClampMin = "0", Units = "s"))
	float OpenDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Animation", meta = (ClampMin = "0", Units = "s"))
	float CloseDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Text")
	FString RotateTextId = TEXT("UI_Document_Rotate");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Inspect|Text")
	FString BackTextId = TEXT("UI_Document_Back");

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Model;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

private:
	enum class EPhase : uint8 { Opening, Open, Closing };

	void RefreshText();
	void ReleaseInput();

	UFUNCTION() void HandleLanguageChanged();
	UFUNCTION() void HandleInputPresentationChanged();
	UFUNCTION() void HandlePromptClicked(UTAActionPromptWidget* Prompt);

	UPROPERTY(Transient) TObjectPtr<UTAItemDefinition> Item;
	UPROPERTY(Transient) TObjectPtr<ATAPreviewStage> PreviewStage;
	UPROPERTY(Transient) TObjectPtr<UTAActionPromptWidget> RotatePrompt;
	UPROPERTY(Transient) TObjectPtr<UTAActionPromptWidget> BackPrompt;

	EPhase Phase = EPhase::Opening;
	float PhaseTime = 0.f;
	bool bGamepadDevice = false;
	bool bDraggingModel = false;
	FVector2D LastDragPosition = FVector2D::ZeroVector;

	FTAInputRouter::FHandle InputRequestHandle = 0;
	TWeakObjectPtr<class AThe_AwakeningPlayerController> InputRequestController;
};
