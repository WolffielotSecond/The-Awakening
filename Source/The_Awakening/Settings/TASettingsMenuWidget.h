#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TAPlayerInputReceiver.h"
#include "Settings/TASettingsTypes.h"
#include "Settings/TASettingsResetHold.h"
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
class UScrollBox;
class UProgressBar;
class UTACommonDialogWidget;
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
 virtual bool ShouldShowPlayerCursor() const override { return !bGamepadDevice; }
 virtual bool AllowsVirtualCursor() const override { return false; }
 virtual UWidget* GetPlayerPointerTarget() const override;
 virtual void NotifyPlayerPointerMoved() override;
 virtual void RemoveFromParent() override;
 void InitializeMenu(AThe_AwakeningPlayerController* PC,UTASettingsMenuWidget* Parent,UTASettingsMenuDefinitionAsset* Menu=nullptr);
 void SetPlayerInputRequest(AThe_AwakeningPlayerController* PC,FTAInputRouter::FHandle Handle);
 UWidget* GetInitialFocusTarget() const;
 UFUNCTION(BlueprintCallable,Category="Settings") void SelectPage(FName PageId);
 void RequestClose();
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
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
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> ScrollBox_SettingsOptions;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> ScrollBox_Description;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UHorizontalBox> HorizontalBox_Controls;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Description;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingTitle;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> Box_Description;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Empty;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> Box_Pages;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RestoreDefaults;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ApplySettings;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ApplySettings;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RestoreDefaults;
 /** Optional authored overlay, below the reset label and above its background. */
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> ProgressBar_ResetHold;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") FLinearColor ResetHoldFillColor=FLinearColor(0.12f,0.42f,0.82f,0.8f);
 /** Seconds of uninterrupted input required to restore the current page. */
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Settings|Reset",meta=(ClampMin="0.01",UIMin="0.01",Units="s")) float ResetHoldDuration=3.f;
 /** Seconds for the ease-out return after release or reset completion. */
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Settings|Reset",meta=(ClampMin="0.01",UIMin="0.01",Units="s")) float ResetReturnDuration=1.f;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ConfirmVideoMode;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RevertVideoMode;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingsPageWidget> PageWidgetClass;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingsSectionWidget> SectionWidgetClass;
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<UTASettingsDescriptionBlockWidget> DescriptionBlockWidgetClass;
 /** Live dead-zone preview inserted for DeadZonePreview description blocks (WBP_DeadZonePreview). */
 UPROPERTY(EditDefaultsOnly,Category="Settings|Style") TSubclassOf<class UTADeadZonePreviewWidget> DeadZonePreviewWidgetClass;
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
 // Local navigation within this owner; does not grant input permission or change ownership.
 enum class ESelectionArea : uint8 { Categories,Details };
 ESelectionArea SelectionArea=ESelectionArea::Categories;
 void EnterDetails();
 enum class EPendingExit : uint8 { None,Page,Submenu,Close,Categories };
 EPendingExit PendingExit=EPendingExit::None;
 FName PendingTarget;
 bool bVideoDialog=false;
 UPROPERTY(Transient) TObjectPtr<UTACommonDialogWidget> ActiveDialog;
 void RequestExit(EPendingExit Kind,FName Target=NAME_None);
 void CompleteExit();
 void ShowVideoDialog();
 void ShowApplyError();
 void ApplyDraftAndContinue();
 UFUNCTION() void ApplyChanges();
 UFUNCTION() void DialogResult(FName Result);
 UFUNCTION() void VideoResolved(bool bKept);
 bool UsesEnglishLayout() const;
 void ApplyLanguageLayout(bool bEnglish);
 struct FFooterLayout { FMargin OuterPadding; FMargin ContentPadding; EVerticalAlignment Vertical; };
 TMap<TWeakObjectPtr<UButton>,FFooterLayout> AuthoredFooterLayout;
#if WITH_DEV_AUTOMATION_TESTS
 friend class FTASettingsMenuInputTest;
 friend class FTASettingsEnglishLayoutTest;
#endif
 void BuildPromptBar();
 FString GetSettingCommandTextId(FName Name) const;
 void TickSliderAdjustment(float DeltaTime,bool Left,bool Right);
 void InitializeResetHoldVisual();
 void TickResetHold(float DeltaTime);
 void CancelResetHold();
 void BuildPageEntries();
 void RefreshPageLabels();
 void OpenSubmenu(const FTASettingDefinition& D);
 void MovePage(int32 Direction);
 void ExecuteSettingsAction(FName Name,bool bGamepadNavigation);
 void BeginGamepadNavigation();
 void SelectPointerRow();
 UScrollBox* GetScrollTarget() const;
 bool CanScrollCurrentRegion() const;
 void RefreshScrollPromptVisibility();
 void ScrollCurrentRegion(int32 Direction);
 void TickScrollInterpolation(float DeltaTime);
 /** Visual scroll interpolation, independent of physical Held state. */
 UPROPERTY(EditDefaultsOnly,Category="Settings|Scroll",meta=(ClampMin="0.1")) float ScrollInterpolationSpeed=12.f;
 TWeakObjectPtr<UScrollBox> AnimatedScroll;
 float ScrollTargetOffset=0.f;
 UFUNCTION() void InputPresentationChanged();
 UFUNCTION() void RowHovered(class UTASelectableMenuOptionWidget* Row);
 UFUNCTION() void RowActivated(UTASettingRowWidget* Row);
 UFUNCTION() void RowAdjusted(UTASettingRowWidget* Row,int32 Direction);
 UFUNCTION() void RowNumberChanged(UTASettingRowWidget* Row,int32 Number);
 UFUNCTION() void RowFavorite(UTASettingRowWidget* Row);
 UFUNCTION() void PromptClicked(UTAActionPromptWidget* Prompt);
 UFUNCTION() void LanguageChanged();
 UFUNCTION() void ValueChanged(FName Id);
 UFUNCTION() void FavoritesChanged();
 UFUNCTION() void ResetPressed();
 UFUNCTION() void ConfirmVideo();
 UFUNCTION() void RevertVideo();
 UPROPERTY(Transient) TArray<FTASettingsPageDefinition> PageDefinitions;
 UPROPERTY(Transient) TArray<TObjectPtr<UTASettingsPageWidget>> PageEntries;
 UPROPERTY(Transient) TArray<TObjectPtr<UTASettingsSectionWidget>> SectionHeadings;
 UPROPERTY(Transient) TArray<TObjectPtr<UUserWidget>> DescriptionEntries;
 TArray<FName> SectionHeadingIds;
 UPROPERTY(Transient) TObjectPtr<UTASettingsMenuWidget> ActiveSubmenu;
 UPROPERTY(Transient) TArray<TObjectPtr<UTAActionPromptWidget>> Prompts;
 bool bPointerSelectionEnabled=true;
 bool bSuppressHoverAfterScroll=false;
 bool bGamepadDevice=false;
 float ScrollRepeatDelay=0;
 float SliderRepeatDelay=0;
 int32 SliderRepeatDirection=0;
 FName SliderRepeatSettingId;
 FName PromptSettingId;
 FTASettingsResetHold ResetHold;
 bool bResetHoldArmed=false;
 bool bGeneratedResetHoldVisual=false;
 float DisplayRefreshDelay=0;
 FTAInputRouter::FHandle InputRequestHandle=0;
};
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTABrightnessMenuWidget : public UTASettingsMenuWidget { GENERATED_BODY() };
