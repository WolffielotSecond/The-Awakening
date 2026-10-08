#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAPlayerInputReceiver.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingsMenuWidget.generated.h"
class AThe_AwakeningPlayerController;
class UTASettingsSubsystem;
class UTASettingRowWidget;
class UTASettingsPageWidget;
class UTASettingsSectionWidget;
class UTASettingsDescriptionBlockWidget;
class UTAActionPromptWidget;
class UInputAction;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTASettingsMenuWidget : public UUserWidget, public ITAPlayerInputReceiver
{
 GENERATED_BODY()
public:
 UTASettingsMenuWidget(const FObjectInitializer& O=FObjectInitializer::Get());
 virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
 virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
 virtual void ExecutePlayerInput(FKey Key,ETAInputCapability Capability) override;
 virtual bool HandleMenuBackRequested() override;
 virtual void RemoveFromParent() override;
 void InitializeMenu(AThe_AwakeningPlayerController* PC,UTASettingsMenuWidget* Parent,UTASettingsMenuDefinitionAsset* Menu=nullptr);
 void SetPlayerInputRequest(AThe_AwakeningPlayerController* PC,FTAInputRouter::FHandle Handle);
 UWidget* GetInitialFocusTarget() const;
 UFUNCTION(BlueprintCallable,Category="Settings") void SelectPage(FName PageId);
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void BuildSettingRows();
 virtual void RefreshRows();
 virtual void ConfirmSelection();
 virtual void RestoreCurrentDefaults();
 UTASettingRowWidget* AddRow(const FTASettingDefinition& D,FText Value=FText::GetEmpty());
 bool Allows(ETAInputCapability C) const;
 FText Text(const FString& Id) const;
 void RefreshSettingDetails(const FTASettingDefinition* Definition);
 void MoveSelection(int32 Direction);
 void AdjustSelectedValue(int32 Direction);
 void ReleaseRequest();
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> Box_SettingsOptions;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UHorizontalBox> HorizontalBox_Controls;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Description;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingTitle;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> Box_Description;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Empty;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> Box_Pages;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RestoreDefaults;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ConfirmVideoMode;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RevertVideoMode;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingsPageWidget> PageWidgetClass;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingsSectionWidget> SectionWidgetClass;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingsDescriptionBlockWidget> DescriptionBlockWidgetClass;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingRowWidget> OptionWidgetClass;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TMap<ETASettingType,TSubclassOf<UTASettingRowWidget>> RowWidgetClasses;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;
 /** Used by a submenu WBP when the entry only supplies a WBP class. */
 UPROPERTY(EditDefaultsOnly,Category="Settings|SubmenuContent") TObjectPtr<UTASettingsMenuDefinitionAsset> DefaultMenuDefinition;
 UPROPERTY(Transient) TObjectPtr<UTASettingsMenuDefinitionAsset> MenuDefinition;
 UPROPERTY(Transient) TObjectPtr<UTASettingsSubsystem> SettingsSubsystem;
 UPROPERTY(Transient) TArray<TObjectPtr<UTASettingRowWidget>> Rows;
 TWeakObjectPtr<AThe_AwakeningPlayerController> InputController;
 TWeakObjectPtr<UTASettingsMenuWidget> ParentMenu;
 FName SelectedSettingId;
 FName CurrentPageId=FTASettingsCatalog::FavoritesPageId();
 int32 SelectedRowIndex=INDEX_NONE;
 void SelectRow(int32 Index);
 void EnsureRowSelection();
private:
 void BuildPromptBar();
 void BuildPageEntries();
 void RefreshPageLabels();
 void OpenSubmenu(const FTASettingDefinition& D);
 void MovePage(int32 Direction);
 UFUNCTION() void RowHovered(class UTASelectableMenuOptionWidget* Row);
 UFUNCTION() void RowActivated(UTASettingRowWidget* Row);
 UFUNCTION() void RowAdjusted(UTASettingRowWidget* Row,int32 Direction);
 UFUNCTION() void RowNumberChanged(UTASettingRowWidget* Row,int32 Number);
 UFUNCTION() void RowFavorite(UTASettingRowWidget* Row);
 UFUNCTION() void PromptClicked(UTAActionPromptWidget* Prompt);
 UFUNCTION() void LanguageChanged();
 UFUNCTION() void ValueChanged(FName Id);
 UFUNCTION() void FavoritesChanged();
 UFUNCTION() void RestoreClicked();
 UFUNCTION() void ConfirmVideo();
 UFUNCTION() void RevertVideo();
 UPROPERTY(Transient) TArray<FTASettingsPageDefinition> PageDefinitions;
 UPROPERTY(Transient) TArray<TObjectPtr<UTASettingsPageWidget>> PageEntries;
 UPROPERTY(Transient) TArray<TObjectPtr<UTASettingsSectionWidget>> SectionHeadings;
 UPROPERTY(Transient) TArray<TObjectPtr<UTASettingsDescriptionBlockWidget>> DescriptionEntries;
 TArray<FName> SectionHeadingIds;
 UPROPERTY(Transient) TObjectPtr<UTASettingsMenuWidget> ActiveSubmenu;
 UPROPERTY(Transient) TArray<TObjectPtr<UTAActionPromptWidget>> Prompts;
 UPROPERTY(Transient) TObjectPtr<UInputAction> PreviousAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> NextAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ConfirmAction;
 FTAInputRouter::FHandle InputRequestHandle=0;
};
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTABrightnessMenuWidget : public UTASettingsMenuWidget { GENERATED_BODY() };
