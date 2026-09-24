#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAActionPromptWidget.generated.h"

class UImage;
class UInputAction;
class UTextBlock;
class UTAInputIconSubsystem;
class UTALocalizeSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAActionPromptClicked, class UTAActionPromptWidget*, Prompt);

/**
 * Reusable widget base for one input-action prompt.
 * Create a Widget Blueprint derived from this class with optional bindings:
 *   Image_Icon       UImage
 *   Text_ActionName  UTextBlock
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAActionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Configure with a ready-to-display name. */
	UFUNCTION(BlueprintCallable, Category = "UI|Action Prompt")
	void ConfigurePrompt(UInputAction* InAction, FText InActionName);

	/** Configure with a localization ID; the widget resolves it and tracks language changes. */
	UFUNCTION(BlueprintCallable, Category = "UI|Action Prompt")
	void ConfigureLocalizedPrompt(UInputAction* InAction, const FString& InActionNameTextId);

	UFUNCTION(BlueprintCallable, Category = "UI|Action Prompt")
	UInputAction* GetPromptAction() const { return PromptAction; }

	UFUNCTION(BlueprintCallable, Category = "UI|Action Prompt")
	FText GetPromptName() const { return ActionName; }

	UFUNCTION(BlueprintCallable, Category = "UI|Action Prompt")
	void RefreshPrompt();

	UPROPERTY(BlueprintAssignable, Category = "UI|Action Prompt")
	FOnTAActionPromptClicked OnPromptClicked;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Action Prompt")
	TObjectPtr<UInputAction> PromptAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Action Prompt")
	FText ActionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Action Prompt", meta = (EditCondition = "bUseLocalizationId"))
	FString ActionNameTextId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Action Prompt")
	bool bUseLocalizationId = false;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ActionName;

	UFUNCTION()
	void HandleInputDeviceChanged();

	UFUNCTION()
	void HandleLanguageChanged();

	void EnsureWidgetBindings();
	void BuildFallbackWidget();

	UPROPERTY(Transient)
	TObjectPtr<UTAInputIconSubsystem> InputIconSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UTALocalizeSubsystem> LocalizeSubsystem;
};
