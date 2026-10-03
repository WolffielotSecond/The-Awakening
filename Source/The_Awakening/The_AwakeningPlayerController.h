// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Core/TAInputRouter.h"
#include "Core/TAInputOwnershipAdapter.h"
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

	bool IsKeyMappedToAction(FKey Key, const UInputAction* Action) const;
	FSimpleMulticastDelegate OnInputOwnerChanged;
	// External ownership lifecycle, independent of Router winner changes.
	FSimpleMulticastDelegate OnPlayerInputOwnershipLost;
	void SynchronizeInputPresentation();
	/** Route a virtual/gamepad confirmation through Slate as a left click without changing input device state. */
	void SimulateSyntheticLeftMouseClick();
	bool IsSimulatingSyntheticLeftMouseClick() const { return bSimulatingSyntheticLeftMouseClick; }
	void SetVirtualCursorAxis(const FKey& AxisKey, float Value);
	bool ConsumeInventoryTogglePress(const UInputAction* Action);
	void TickVirtualCursor(float DeltaTime);
	bool RoutePlayerInputKey(FKey Key, bool bRepeat, int32 UserIndex);
	FVector ReadHeldKey(FKey Key) const { return HeldKeyValues.FindRef(Key); }
	void NotifyApplicationActivationChanged(bool bIsActive);

	bool UsesCursorLook() const;
	bool IsCursorStickLookActive() const { return UsesCursorLook() && (!VirtualCursorAxis.IsNearlyZero() || !RightCursorAxis.IsNearlyZero()); }
	bool GetPlayerCursorPosition(float& X, float& Y) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	// Serialized template name retained for existing desktop IMC_MouseLook defaults.
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings", meta = (DisplayName = "Additional Input Mapping Contexts"))
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	TSharedPtr<FTAInputDeviceDetector> InputDeviceDetector;
	FDelegateHandle ApplicationActivationHandle;
	FDelegateHandle DeviceConnectionHandle;
	TMap<FKey, FVector> HeldKeyValues;
	FTAInputRouter InputRouter;
	// Output deduplication only. Never used to restore policy or grant permission.
	TOptional<FTAInputRouter::FHandle> ObservedWinnerHandle;
	TOptional<FTAInputRouter::FHandle> PresentedRequestHandle;
	TWeakObjectPtr<UWidget> PresentedFocusTarget;
	TWeakObjectPtr<UWidget> PresentedCursorOwner;
	TWeakPtr<class SViewport> PresentedViewport;
	uint64 InternalFocusTransitionUntil = 0;
	TSet<FKey> ConsumedInventoryKeys;
	bool bApplicationInputActive = true;
	UPROPERTY(Transient)
	TMap<TObjectPtr<UInputModifier>, TObjectPtr<UInputModifier>> HeldInputModifiers;
	bool bSimulatingSyntheticLeftMouseClick = false;
	FVector2D VirtualCursorAxis = FVector2D::ZeroVector;
	FVector2D RightCursorAxis = FVector2D::ZeroVector;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void ObservePlayerInputOwnership(TAInputOwnershipAdapter::EState State);
	// Notification edge memory only; never queried to grant permission or restore modes.
	TOptional<TAInputOwnershipAdapter::EState> LastDefinitiveInputOwnership;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTAExternalInputOwnershipTest;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scan", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTAScanningComponent> ScanningComponent;
};
