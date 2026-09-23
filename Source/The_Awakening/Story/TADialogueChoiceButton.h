// Source/The_Awakening/Story/TADialogueChoiceButton.h
// 分支选项按钮（WBP_DialogueChoiceButton 的 C++ 基类）
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TADialogueChoiceButton.generated.h"

class UButton;
class UTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStoryChoiceClicked, int32, Index);

/**
 * 控件命名约定（WBP_DialogueChoiceButton）：
 *   Button_Choice（UButton，含样式）
 *   Text_Choice（UTextBlock）
 *   Image_Highlight（UImage，可选，选中高亮边框）
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADialogueChoiceButton : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** 设置文本与可见序号（序号用于点击回调） */
	void Setup(int32 InIndex, const FText& Text);

	/** 高亮（当前选中项） */
	void SetHighlighted(bool bHighlighted);

	/** 点击回调（对话 UI 绑定） */
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnStoryChoiceClicked OnClickedIndex;

protected:
	UFUNCTION()
	void HandleButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Choice;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Choice;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Highlight;

	int32 Index = INDEX_NONE;
};
