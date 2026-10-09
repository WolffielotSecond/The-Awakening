#pragma once

#include "CoreMinimal.h"
#include "Core/TAPlayerInputReceiver.h"
#include "Blueprint/UserWidget.h"
#include "TAPauseMenuWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UTAPauseMenuOptionWidget;
class UTAActionPromptWidget;
class UInputAction;
class UTAInputIconSubsystem;
class AThe_AwakeningPlayerController;
class UWidget;
class UTASelectableMenuOptionWidget;

UENUM(BlueprintType)
enum class ETAPauseMenuAction : uint8
{
	Resume,
	OpenSettings,
	QuitGame
};

USTRUCT(BlueprintType)
struct FTAPauseMenuOption
{
	GENERATED_BODY()

	FTAPauseMenuOption() = default;
	FTAPauseMenuOption(FName InOptionId, FString InLocalizationId, FText InLabel, ETAPauseMenuAction InAction)
		: OptionId(InOptionId)
		, LocalizationId(MoveTemp(InLocalizationId))
		, Label(MoveTemp(InLabel))
		, Action(InAction)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FName OptionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FString LocalizationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	ETAPauseMenuAction Action = ETAPauseMenuAction::Resume;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTAOnPauseMenuSettingsRequested);

/** Data-driven pause menu. Blueprint controls layout; this class builds rows and executes actions. */
UCLASS(Abstract)
class THE_AWAKENING_API UTAPauseMenuWidget : public UUserWidget, public ITAPlayerInputReceiver
{
	GENERATED_BODY()

public:
	UTAPauseMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return InputRequestHandle; }
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) override;
	virtual bool HandleMenuBackRequested() override;
	void SetPlayerInputRequest(AThe_AwakeningPlayerController* Controller, FTAInputRouter::FHandle Handle);
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void RemoveFromParent() override;

	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FTAOnPauseMenuSettingsRequested OnSettingsRequested;

	/** Called by the owning player controller when the player selects Continue. */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void RequestResume();
	UWidget* GetInitialFocusTarget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Place and style this container in the pause-menu Widget Blueprint. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Options;

	/** Place this prompt bar at the bottom of the pause menu in its Widget Blueprint. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Controls;

	/** Set this to a styled Widget Blueprint based on UTAPauseMenuOptionWidget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause Menu|Options")
	TSubclassOf<UTAPauseMenuOptionWidget> OptionWidgetClass;

	/** Designers can reorder, rename, remove, or extend the menu without changing C++. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause Menu|Options")
	TArray<FTAPauseMenuOption> Options;

	UPROPERTY(EditAnywhere, Category = "Pause Menu|Input")
	TObjectPtr<UInputAction> PreviousOptionAction;

	UPROPERTY(EditAnywhere, Category = "Pause Menu|Input")
	TObjectPtr<UInputAction> NextOptionAction;

	UPROPERTY(EditAnywhere, Category = "Pause Menu|Input")
	TObjectPtr<UInputAction> ConfirmOptionAction;

	UPROPERTY(EditAnywhere, Category = "Pause Menu|Input")
	TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;

private:
	void ApplyLanguageLayout(bool bEnglish);
	bool UsesEnglishLayout() const;
	TOptional<FMargin> AuthoredOptionsOffsets;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTAEnglishUIBoundsTest;
#endif
	void BuildOptions();
	void ValidateInputActions() const;
	void BuildActionPromptBar();
	void RefreshOptionHighlights();
	void MoveSelection(int32 Delta);
	void ConfirmSelection();
	void RefreshOptionLabels();
	UFUNCTION()
	void HandleOptionSelected(FName OptionId);
	UFUNCTION()
	void HandleOptionHovered(UTASelectableMenuOptionWidget* OptionWidget);
	void SelectOptionById(FName OptionId);
	FText GetOptionLabel(const FTAPauseMenuOption& Option) const;

	TMap<FName, ETAPauseMenuAction> ActionsByOptionId;
	TMap<FName, TObjectPtr<UTAPauseMenuOptionWidget>> OptionWidgetsById;

	UFUNCTION()
	void HandleLanguageChanged();

	UFUNCTION()
	void HandleActionPromptClicked(UTAActionPromptWidget* Prompt);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTAActionPromptWidget>> ActionPromptWidgets;
	TWeakObjectPtr<AThe_AwakeningPlayerController> InputRequestController;
	FTAInputRouter::FHandle InputRequestHandle = 0;
	int32 SelectedOptionIndex = 0;
};
