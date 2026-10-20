// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Core/TAInputRouter.h"
#include "Core/TAInputOwnershipAdapter.h"
#include "Components/SlateWrapperTypes.h"
#include "UI/Cursor/TACursorSettings.h"
#include "Core/TAGamepadDeadZone.h"
#include "The_AwakeningPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UWidget;
class UTAScanningComponent;
class UInputAction;
class UInputModifier;
class SViewport;
class FWidgetPath;
class UTAPauseMenuWidget;
class UTASettingsMenuWidget;
class UTAAdvancedDataWidget;
class UTACommonDialogWidget;
class UTAHealthHUDWidget;
struct FInputActionValue;
struct FInputKeyEventArgs;
class FTAGameCursor;
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
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;

private:
	AThe_AwakeningPlayerController* Owner = nullptr;
	TSet<FKey> ConsumedPresses;
	// Stick-direction / trigger keys pressed inside the dead zone, and those accepted after leaving it.
	TSet<FKey> DeadZoneIgnoredPresses;
	TSet<FKey> DeadZoneAcceptedPresses;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTAHeldObservationLifecycleTest;
	friend class FTAGamepadDeadZoneInputTest;
#endif
};

UCLASS(abstract)
class AThe_AwakeningPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AThe_AwakeningPlayerController();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cursor")
	FTACursorStyleSettings CursorStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HOVER PULSE SETTINGS")
	FTACursorPulseSettings HoverPulseSettings;
	/** Console command: TACursorTest toggles the interactive tuning screen. */
	UFUNCTION(Exec)
	void TACursorTest();
	void UpdateGameCursor();
	void NotifyCursorButton(bool bDown);
	bool OwnsPlayerInput(const FVector2D* Pointer = nullptr) const;
	bool AllowsInput(ETAInputCapability Capability) const;
	bool AllowsInputFor(FTAInputRouter::FHandle Handle, const UObject* RequestOwner, ETAInputCapability Capability) const;
	void RefreshInputOwnership();
	/** Explicit ownership; presentation is derived from the winning request. */
	FTAInputRouter::FHandle AcquireInputRequest(const FTAInputRequest& Request);
	void ReleaseInputRequest(FTAInputRouter::FHandle Handle);
	FTAInputRouter::FWinner GetInputWinner() const;
	void NotifyRawInputKey(const FKey& Key);
	/** Observe held inputs before UI consumes them, separately from gameplay permission. */
	void RecordHeldInput(FKey Key, float Value, int32 UserIndex);
	FInputActionValue ReadHeldAction(const UInputAction* Action);
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void OpenPauseMenu();
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void ClosePauseMenu();
	UFUNCTION(BlueprintPure, Category = "UI|Pause Menu")
	bool IsPauseMenuOpen() const;
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void OpenSettingsMenu();
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettingsMenu(bool bForce=false);
 void CancelSettingsCloseRequest() { bClosePauseAfterSettings=false; }
 UInputAction* GetDialogAction(int32 Slot) const { return DialogActions.FindRef(Slot); }
 TSubclassOf<UTACommonDialogWidget> GetCommonDialogWidgetClass() const;
	UInputAction* GetUIBackAction() const { return UIBackAction; }
	UInputAction* GetPauseAction() const { return PauseAction; }
 UInputAction* GetSettingsAction(FName Name) const { return SettingsActions.FindRef(Name); }

	bool IsKeyMappedToAction(FKey Key, const UInputAction* Action) const;
	FSimpleMulticastDelegate OnInputOwnerChanged;
	// External ownership lifecycle, independent of Router winner changes.
	FSimpleMulticastDelegate OnPlayerInputOwnershipLost;
	void SynchronizeInputPresentation();
	/** Route a virtual/gamepad confirmation through Slate as a left click without changing input device state. */
	void SimulateSyntheticLeftMouseClick();
	bool IsSimulatingSyntheticLeftMouseClick() const { return bSimulatingSyntheticLeftMouseClick; }
	bool ConsumeInventoryTogglePress(const UInputAction* Action);
	void TickVirtualCursor(float DeltaTime);
	bool RoutePlayerInputKey(FKey Key, bool bRepeat, int32 UserIndex);
	FVector ReadHeldKey(FKey Key) const { return HeldKeyValues.FindRef(Key); }
	/** Missing observation is unknown, not a physical release. */
	TOptional<FVector> FindHeldKeyObservation(FKey Key) const;
	/** Nonzero physical sources among resolved mappings; does not evaluate modifiers/triggers. */
	TArray<FKey> GetObservedHeldKeysForAction(const UInputAction* Action) const;
	/**
	 * 动作映射的按键在物理观测中已明确松开：至少有一个映射键被观测到，且没有任何映射键按着。
	 * 没有观测记录（例如失去输入所有权后）不算松开。界面接收了松开事件、动作没收到 Completed 时用它补判。
	 */
	bool IsActionObservedReleased(const UInputAction* Action) const;
	void NotifyApplicationActivationChanged(bool bIsActive);

	bool UsesCursorLook() const;
	bool IsCursorStickLookActive() const;
	/** Player dead zones; raw values feed the settings preview. */
	const FTAGamepadDeadZone& GetGamepadDeadZone() const { return GamepadDeadZone; }
	/** Reads the applied dead-zone settings. Called once per frame by the input processor. */
	void RefreshGamepadDeadZones();
	bool GetPlayerCursorPosition(float& X, float& Y) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TSoftObjectPtr<UInputMappingContext> PickupInputMappingContext{FSoftObjectPath(TEXT("/Game/Input/IMC_Pickup.IMC_Pickup"))};

	// Serialized template name retained for existing desktop IMC_MouseLook defaults.
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings", meta = (DisplayName = "Additional Input Mapping Contexts"))
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;
	/** Shared UI mappings (inventory, dialogue, pause, and future menus), active for this local player. */
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TObjectPtr<UInputMappingContext> UIInputMappingContext;
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TObjectPtr<UInputMappingContext> SettingsInputMappingContext;
 UPROPERTY(EditAnywhere,Category="Input|Input Mappings") TObjectPtr<UInputMappingContext> CommonDialogInputMappingContext;
 UPROPERTY() TSoftObjectPtr<UInputMappingContext> DefaultCommonDialogContext{FSoftObjectPath(TEXT("/Game/Input/IMC_CommonDialog.IMC_CommonDialog"))};
 UPROPERTY() TSoftClassPtr<UTACommonDialogWidget> DefaultCommonDialogClass{FSoftObjectPath(TEXT("/Game/UI/Common/WBP_CommonDialog.WBP_CommonDialog_C"))};
 UPROPERTY(EditAnywhere,Category="UI|Dialog") TSubclassOf<UTACommonDialogWidget> CommonDialogWidgetClass;
 UPROPERTY(Transient) TMap<int32,TObjectPtr<UInputAction>> DialogActions;
	UPROPERTY(EditAnywhere, Category = "Input|Input Actions")
	TObjectPtr<UInputAction> UIBackAction;
	UPROPERTY(EditAnywhere, Category = "Input|Input Actions")
	TObjectPtr<UInputAction> PauseAction;
	UPROPERTY(EditAnywhere, Category = "UI|Pause Menu")
	TSubclassOf<UTAPauseMenuWidget> PauseMenuWidgetClass;
	UPROPERTY(EditAnywhere, Category = "UI|Settings")
	TSubclassOf<UTASettingsMenuWidget> SettingsMenuWidgetClass;
 /** Automatically added to this local player's screen; layout belongs to the WBP. */
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="UI|Advanced Data")
 TSoftClassPtr<UTAAdvancedDataWidget> AdvancedDataWidgetClass{FSoftObjectPath(TEXT("/Game/UI/WBP_AdvancedData.WBP_AdvancedData_C"))};
 UPROPERTY(Transient)
 TObjectPtr<UTAAdvancedDataWidget> AdvancedDataWidgetInstance;
	/** 血量 HUD，BeginPlay 时加到本地玩家屏幕；没配或资源不存在时不创建，只打 Warning。布局归 WBP。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Health")
	TSoftClassPtr<UTAHealthHUDWidget> HealthHUDWidgetClass{FSoftObjectPath(TEXT("/Game/UI/HUD/WBP_HealthHUD.WBP_HealthHUD_C"))};
	UPROPERTY(EditDefaultsOnly, Category = "UI|Health")
	int32 HealthHUDZOrder = 10;
	UPROPERTY(Transient)
	TObjectPtr<UTAHealthHUDWidget> HealthHUDWidgetInstance;
	UPROPERTY(Transient)
	TObjectPtr<UTAPauseMenuWidget> PauseMenuInstance;
	UPROPERTY(Transient)
	TObjectPtr<UTASettingsMenuWidget> SettingsMenuInstance;
 UPROPERTY(Transient)
 TMap<FName,TObjectPtr<UInputAction>> SettingsActions;

	TSharedPtr<FTAInputDeviceDetector> InputDeviceDetector;
	TSharedPtr<FTAGameCursor> GameCursor;
	FDelegateHandle ApplicationActivationHandle;
	FDelegateHandle DeviceConnectionHandle;
	TMap<FKey, FVector> HeldKeyValues;
	FTAGamepadDeadZone GamepadDeadZone;
	// Virtual stick/trigger keys whose press reached Enhanced Input.
	TSet<FKey> DeadZonePassedKeyDowns;
	FTAInputRouter InputRouter;
	// Output deduplication only. Never used to restore policy or grant permission.
	TOptional<FTAInputRouter::FHandle> ObservedWinnerHandle;
	TOptional<FTAInputRouter::FHandle> PresentedRequestHandle;
	TWeakObjectPtr<UWidget> PresentedFocusTarget;
	TWeakObjectPtr<UWidget> PresentedCursorOwner;
	TWeakPtr<class SViewport> PresentedViewport;
	TSet<FKey> ConsumedInventoryKeys;
	bool bApplicationInputActive = true;
	UPROPERTY(Transient)
	TMap<TObjectPtr<UInputModifier>, TObjectPtr<UInputModifier>> HeldInputModifiers;
	bool bSimulatingSyntheticLeftMouseClick = false;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	void HandlePauseAction(const FInputActionValue& Value);
	void CreateHealthHUD();
	UFUNCTION()
	void HandleSettingsRequested();

private:
	friend class FTAInputDeviceDetector;
	// An active host window alone does not authorize a synthetic pointer target.
	static bool IsSyntheticClickPathWithinPlayerSurface(const FWidgetPath& Path, const TSharedPtr<SViewport>& Surface);
	TAInputOwnershipAdapter::EState GetPlayerInputOwnershipState() const;
	// The processor supplies the adapter classification; no synthetic zero on a focus gap.
	bool ObserveAnalogInput(FKey Key, float Value, int32 UserIndex, TAInputOwnershipAdapter::EState State);
	// Pure consumer interpretation of Held axes; no second physical-axis state.
	FVector2D ReadCursorStick(FKey PairedAxisKey) const;
	FVector2D GetCursorInputAxis();
	// Observation invalidation is not a physical release or a capability decision.
	// Processor Down/Up pairing survives both paths until its matching Up/teardown.
	void InvalidatePhysicalObservationForExternalOwnership();
	// Existing disconnect policy: gamepad keys for the matching local platform user,
	// not keyboard/mouse, accepted movement, or held modifier instances.
	void InvalidateGamepadObservationOnDisconnect();
	void ObservePlayerInputOwnership(TAInputOwnershipAdapter::EState State);
	void SynchronizeUIInputMappingContext(const FTAInputRouter::FWinner& Winner);
	bool bUIInputMappingContextActive = false;
	bool bSettingsInputMappingContextActive = false;
 bool bClosePauseAfterSettings=false;
 bool bDialogInputMappingContextActive=false;
	bool bSynchronizingInputPresentation=false;
	TOptional<FVector2D> LastVirtualCursorPosition;
	// Lifecycle edge memory (loss notification/invalidation), never an authorization source.
	TOptional<TAInputOwnershipAdapter::EState> LastDefinitiveInputOwnership;
	FTAInputRouter::FHandle PauseInputRequestHandle = 0;
	// Retain the pause page and its input request while Settings is presented.
	ESlateVisibility PauseVisibilityBeforeSettings = ESlateVisibility::Visible;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTAExternalInputOwnershipTest;
	friend class FTASettingsMenuInputTest;
 friend class FTASettingsDialogTest;
	friend class FTAHeldObservationLifecycleTest;
	friend class FTAVirtualCursorAxesTest;
	friend class FTASyntheticClickSurfaceTest;
	friend class FTAParkourPhysicalReleaseTest;
	friend class FTAGamepadDeadZoneInputTest;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scan", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTAScanningComponent> ScanningComponent;
};
