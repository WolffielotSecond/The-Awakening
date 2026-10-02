// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Core/TAInputRouter.h"
#include "The_AwakeningPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UWidget;
class UTAScanningComponent;
class UInputAction;
class UInputModifier;
struct FInputActionValue;
struct FInputKeyEventArgs;
/**
 * 输入设备检测器
 */
class FTAInputDeviceDetector : public IInputProcessor
{
public:
	FTAInputDeviceDetector(class AThe_AwakeningPlayerController* InOwner)
		: Owner(InOwner)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SoftApp, TSharedRef<ICursor> Cursor) override;

	virtual bool HandleKeyDownEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SoftApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;

private:
	AThe_AwakeningPlayerController* Owner = nullptr;
	TSet<FKey> ConsumedPresses;
};

UCLASS(abstract)
class AThe_AwakeningPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AThe_AwakeningPlayerController();
	bool OwnsPlayerInput(const FVector2D* Pointer = nullptr) const;
	bool AllowsInput(ETAInputCapability Capability) const;
	void RefreshInputOwnership();
	void NotifyRawInputKey(const FKey& Key);
	/** Observe held inputs before UI consumes them, separately from gameplay permission. */
	void RecordHeldInput(FKey Key, float Value, int32 UserIndex);
	FInputActionValue ReadHeldAction(const UInputAction* Action);
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** 进入/退出对话输入模式（移除/恢复默认移动等映射，供剧情系统调用） */
	void SetDialogueModeActive(bool bActive, UUserWidget* FocusWidget = nullptr);

	/** 所有可交互 UI 共用的输入模式。调用需成对，支持多个 UI 同时打开。 */
	void BeginUIInputMode(UUserWidget* FocusWidget = nullptr);
	void EndUIInputMode(UUserWidget* RequestOwner = nullptr);
	void SetUIFocusWidget(UUserWidget* FocusWidget);
	bool IsUIInputModeActive() const { return bUIInputModeActive; }
	/** Route a virtual/gamepad confirmation through Slate as a left click without changing input device state. */
	void SimulateSyntheticLeftMouseClick();
	bool IsSimulatingSyntheticLeftMouseClick() const { return bSimulatingSyntheticLeftMouseClick; }
	void SetVirtualCursorAxis(const FKey& AxisKey, float Value);
	bool ConsumeInventoryTogglePress(const UInputAction* Action);
	void TickVirtualCursor(float DeltaTime);
	void SetPuzzleScopeWidget(class UTAPathPuzzleWidget* Widget);
	FVector2D GetPuzzlePanInput() const;
	bool HandlePuzzleConfirm(FKey Key, bool bRepeat, int32 UserIndex);
	bool HandlePuzzleUndo(FKey Key, bool bRepeat, int32 UserIndex);
	void NotifyApplicationActivationChanged(bool bIsActive);

	/** Scan only requests cursor visibility; it does not enter menu input mode. */
	void BeginScanCursorMode();
	void EndScanCursorMode();
	bool IsScanCursorModeActive() const { return bScanCursorModeActive; }
	bool IsScanStickCursorActive() const { return bScanCursorModeActive && !bUIInputModeActive && (!VirtualCursorAxis.IsNearlyZero() || !ScanRightCursorAxis.IsNearlyZero()); }
	bool GetScanCursorPosition(float& X, float& Y) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	TObjectPtr<UUserWidget> MobileControlsWidget;

	TSharedPtr<FTAInputDeviceDetector> InputDeviceDetector;
	FDelegateHandle ApplicationActivationHandle;
	FDelegateHandle DeviceConnectionHandle;
	TMap<FKey, FVector> HeldKeyValues;
	FTAInputRouter InputRouter;
	TArray<FTAInputRouter::FHandle> MenuInputHandles;
	TArray<TWeakObjectPtr<UUserWidget>> MenuInputOwners;
	TWeakObjectPtr<UUserWidget> DialogueInputOwner;
	FTAInputRouter::FHandle ScanInputHandle = 0;
	FTAInputRouter::FHandle PuzzleInputHandle = 0;
	uint64 InternalFocusTransitionUntil = 0;
	TSet<FKey> ConsumedInventoryKeys;
	bool bApplicationInputActive = true;
	UPROPERTY(Transient)
	TMap<TObjectPtr<UInputModifier>, TObjectPtr<UInputModifier>> HeldInputModifiers;
	int32 ActiveUIModeCount = 0;
	bool bUIInputModeActive = false;
	TWeakObjectPtr<class UTAPathPuzzleWidget> PuzzleScopeWidget;
	bool bDialogueModeActive = false;
	bool bPreviousShowMouseCursor = false;
	bool bMouseCursorBeforeScan = false;
	bool bScanCursorModeActive = false;
	bool bSimulatingSyntheticLeftMouseClick = false;
	FVector2D VirtualCursorAxis = FVector2D::ZeroVector;
	FVector2D ScanRightCursorAxis = FVector2D::ZeroVector;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scan", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTAScanningComponent> ScanningComponent;
};
