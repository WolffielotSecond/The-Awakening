#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "Puzzle/TAPathPuzzleSession.h"
#include "TAPathPuzzleWidget.generated.h"

class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UButton;
class UTAPathPuzzleNodeWidget;

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPuzzleAppearance
{
	GENERATED_BODY()
	FTAPuzzleAppearance();
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush NodeNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush NodeEndpoint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush NodeSelected;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush NodeFailed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush EdgeNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush EdgeSelected;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSlateBrush EdgeFailed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor TimerColor = FLinearColor(.1f, .8f, .2f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D BoardSize = FVector2D(820, 535);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D BoardPadding = FVector2D(55, 35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D NodeSize = FVector2D(78, 52);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float EdgeThickness = 4.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTAOnPuzzleRewardApplied, const FTAPuzzleEffect&, Effect, int32, AppliedAmount);

/** Inherit in a Widget Blueprint. All generated visuals are children of the independent PuzzleCanvas. */
UCLASS()
class THE_AWAKENING_API UTAPathPuzzleWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Creates the selected WBP class, binds its configured defaults, and adds it to the viewport. */
	UFUNCTION(BlueprintCallable, Category="Puzzle", meta=(DefaultToSelf="Player"))
	static UTAPathPuzzleWidget* OpenPuzzle(APlayerController* Player, TSubclassOf<UTAPathPuzzleWidget> WidgetClass,
		UObject* RewardReceiver = nullptr, int32 Seed = -1);

	/** Generates and opens a puzzle using explicit per-entry settings instead of WBP gameplay defaults.
	 * Split the Settings pin in Blueprint to configure difficulty, effects and usage allowances.
	 * Visual defaults still come from WidgetClass. Returns nullptr if creation fails.
	 */
	UFUNCTION(BlueprintCallable, Category="Puzzle", meta=(DefaultToSelf="Player", AdvancedDisplay="RewardReceiver,Seed"))
	static UTAPathPuzzleWidget* OpenPuzzleWithSettings(APlayerController* Player,
		TSubclassOf<UTAPathPuzzleWidget> WidgetClass, const FTAPuzzleSettings& Settings,
		UObject* RewardReceiver = nullptr, int32 Seed = -1);

	/** One session per widget instance. Create a new widget when re-entering the minigame. */
	UFUNCTION(BlueprintCallable, Category="Puzzle") bool StartPuzzle();
	/** Close the minigame, release its freeze request and restore gameplay input. */
	UFUNCTION(BlueprintCallable, Category="Puzzle") void ClosePuzzle();
	UFUNCTION(BlueprintCallable, Category="Puzzle") void Undo();
	UFUNCTION(BlueprintCallable, Category="Puzzle") void Retry();
	UFUNCTION(BlueprintCallable, Category="Puzzle") void SelectNode(int32 NodeIndex);
	UFUNCTION(BlueprintCallable, Category="Puzzle") void RefreshBoard();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle", meta=(ExposeOnSpawn="true")) FTAPuzzleSettings PuzzleSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle", meta=(ExposeOnSpawn="true")) int32 PuzzleSeed = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") FTAPuzzleAppearance Appearance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") TSubclassOf<UTAPathPuzzleNodeWidget> NodeWidgetClass;
	/** If supplied, must implement TAPuzzleRewardReceiver; otherwise built-in money/inventory/GAS applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle|Rewards", meta=(ExposeOnSpawn="true")) TObjectPtr<UObject> RewardReceiver;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") bool bAutoStart = true;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") TObjectPtr<UTAPathPuzzleSession> Session;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") FText SetupError;
	UPROPERTY(BlueprintAssignable, Category="Puzzle") FTAOnPuzzleSettled OnSucceeded;
	UPROPERTY(BlueprintAssignable, Category="Puzzle") FTAOnPuzzleSettled OnFailed;
	/** Includes partial/failed delivery (e.g. full inventory). No automatic retry that could duplicate grants. */
	UPROPERTY(BlueprintAssignable, Category="Puzzle|Rewards") FTAOnPuzzleRewardApplied OnRewardApplied;
	UPROPERTY(BlueprintAssignable, Category="Puzzle") FTAOnPuzzleChanged OnAborted;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Optional WBP controls. PuzzleCanvas is required when supplying a custom designer root. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Puzzle|UI") TObjectPtr<UCanvasPanel> PuzzleCanvas;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> Progress_Time;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Stats;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Path;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Effects;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Time;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Undo;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Retry;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Undo;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Retry;

private:
	FTimerHandle SettlementCloseTimer;
	static UTAPathPuzzleWidget* OpenPuzzleInternal(APlayerController* Player,
		TSubclassOf<UTAPathPuzzleWidget> WidgetClass, const FTAPuzzleSettings* Settings,
		UObject* RewardReceiver, int32 Seed);
	void BuildFallback();
	void RefreshChrome();
	void ReleaseInput();
	UFUNCTION() void HandleChanged();
	UFUNCTION() void HandleSettled(const FTAPuzzleResult& Result);
	bool bOwnsInputMode = false;
	bool bSettlementDelivered = false;
	UPROPERTY() TObjectPtr<UObject> SessionRewardReceiver;
};
