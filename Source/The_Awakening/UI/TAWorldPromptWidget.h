#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAWorldPromptWidget.generated.h"

class UImage;
class UTextBlock;
class USizeBox;
class UTexture2D;

/**
 * 世界交互提示控件的 C++ 父类（推荐给 WBP_InteractPrompt 设为父类）。布局和样式都在 WBP 里；
 * C++ 只按名字绑定可选控件并填入图标与文字。出现 / 切换内容的动画写在 OnPromptChanged。
 */
UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTAWorldPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPrompt(UTexture2D* KeyIcon, const FText& PromptText, float IconHeight);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Prompt")
	void OnPromptChanged(UTexture2D* KeyIcon, const FText& PromptText);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Key;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Prompt;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_Key;
};
