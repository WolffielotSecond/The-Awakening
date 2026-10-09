#pragma once

#include "CoreMinimal.h"
#include "UI/TASelectableMenuOptionWidget.h"
#include "TAPauseMenuOptionWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAOnPauseMenuOptionSelected, FName, OptionId);

/** One styled, Blueprint-laid-out row in the data-driven pause menu. */
UCLASS(Abstract)
class THE_AWAKENING_API UTAPauseMenuOptionWidget : public UTASelectableMenuOptionWidget
{
	GENERATED_BODY()

public:
	void ConfigureOption(FName InOptionId, const FText& InLabel);
	void ApplyEnglishLayout(bool bEnglish);
	FName GetOptionId() const { return CurrentOptionId; }
	void SetHighlighted(bool bHighlighted);
	UWidget* GetFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FTAOnPauseMenuOptionSelected OnOptionSelected;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Option;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Option;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> Image_Highlight;

private:
	TOptional<FMargin> AuthoredContentPadding;
	UFUNCTION()
	void HandleButtonClicked();

	FName CurrentOptionId;
	FLinearColor UnselectedBackgroundColor = FLinearColor::White;
	bool bHasCachedButtonBackground = false;
};
