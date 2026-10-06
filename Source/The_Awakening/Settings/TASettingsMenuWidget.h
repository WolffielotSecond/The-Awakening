#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAPlayerInputReceiver.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingsMenuWidget.generated.h"

class AThe_AwakeningPlayerController;
class UTASettingsSubsystem;
class UTAPauseMenuOptionWidget;
class UTAActionPromptWidget;
class UInputAction;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTASettingsMenuWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	UTASettingsMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override;
	virtual void RemoveFromParent() override;

	void InitializeMenu(AThe_AwakeningPlayerController* InController, UTASettingsMenuWidget* InParent,
		ETASettingSubmenuTarget InSubmenuTarget = ETASettingSubmenuTarget::None);
	void SetPlayerInputRequest(AThe_AwakeningPlayerController* InController, FTAInputRouter::FHandle Handle);
	UWidget* GetInitialFocusTarget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Pages;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_SettingsOptions;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Data")
	TObjectPtr<UTASettingsDefinitionAsset> DefinitionAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Style")
	TSubclassOf<UTAPauseMenuOptionWidget> OptionWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Style")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Input")
	TObjectPtr<UInputAction> PreviousAction;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Input")
	TObjectPtr<UInputAction> NextAction;

	UPROPERTY(EditDefaultsOnly, Category = "Settings|Input")
	TObjectPtr<UInputAction> ConfirmAction;

private:
	void BuildPageButtons();
	void BuildSettingRows();
	void BuildPromptBar();
	void RefreshRows();
	void SelectPage(ETAGameSettingsPage Page);
	void MoveSelection(int32 Direction);
	void AdjustSelectedValue(int32 Direction);
	void ConfirmSelection();
	void OpenSubmenu(const FTASettingDefinition& Definition);
	void CloseSubmenu();
	FText ResolveText(const FString& TextId) const;
	FText GetSettingRowLabel(const FTASettingDefinition& Definition) const;
	FText GetPageLabel(ETAGameSettingsPage Page) const;

	UFUNCTION()
	void HandleOptionHovered(class UTASelectableMenuOptionWidget* OptionWidget);

	UFUNCTION()
	void HandleOptionSelected(FName OptionId);

	UFUNCTION()
	void HandleActionPromptClicked(UTAActionPromptWidget* Prompt);

	UFUNCTION()
	void HandleLanguageChanged();

	UFUNCTION()
	void HandleSettingValueChanged(FName SettingId);

	TWeakObjectPtr<AThe_AwakeningPlayerController> InputController;
	TWeakObjectPtr<UTASettingsMenuWidget> ParentMenu;
	UPROPERTY(Transient)
	TObjectPtr<UTASettingsMenuWidget> ActiveSubmenu;
	TObjectPtr<UTASettingsSubsystem> SettingsSubsystem;
	TMap<FName, TObjectPtr<UTAPauseMenuOptionWidget>> PageWidgets;
	TMap<FName, TObjectPtr<UTAPauseMenuOptionWidget>> SettingWidgets;
	TArray<TObjectPtr<UTAActionPromptWidget>> ActionPromptWidgets;
	FTAInputRouter::FHandle InputRequestHandle = 0;
	ETAGameSettingsPage CurrentPage = ETAGameSettingsPage::Game;
	ETASettingSubmenuTarget SubmenuTarget = ETASettingSubmenuTarget::None;
	int32 SelectedSettingIndex = 0;
	int32 SelectedPageIndex = 0;
	FName SelectedSettingId = NAME_None;
};
