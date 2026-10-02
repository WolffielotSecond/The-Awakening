// Copyright Epic Games, Inc. All Rights Reserved.

#include "The_AwakeningPlayerController.h"
#include "Scan/TAScanningComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "EnhancedPlayerInput.h"
#include "Core/TAPlayerInput.h"
#include "Core/TAInputOwnershipAdapter.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Blueprint/UserWidget.h"
#include "The_Awakening.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Framework/Application/SlateApplication.h"
#include "Core/TAInputIconSubsystem.h"
#include "InputKeyEventArgs.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "The_AwakeningCharacter.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/NavigationConfig.h"
#include "Widgets/SWindow.h"


AThe_AwakeningPlayerController::AThe_AwakeningPlayerController()
{
	OverridePlayerInputClass = UTAPlayerInput::StaticClass();
	ScanningComponent = CreateDefaultSubobject<UTAScanningComponent>(TEXT("ScanComponent"));
}

bool FTAInputDeviceDetector::HandleKeyDownEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent)
{
	if (!Owner || !Owner->OwnsPlayerInput()) return false;
	if (Owner)
	{
		Owner->NotifyRawInputKey(InKeyEvent.GetKey());
		Owner->RecordHeldInput(InKeyEvent.GetKey(), 1.f, InKeyEvent.GetUserIndex());
		if (Owner->HandlePuzzleConfirm(InKeyEvent.GetKey(), InKeyEvent.IsRepeat(), InKeyEvent.GetUserIndex()) ||
			Owner->HandlePuzzleUndo(InKeyEvent.GetKey(), InKeyEvent.IsRepeat(), InKeyEvent.GetUserIndex()))
		{ ConsumedPresses.Add(InKeyEvent.GetKey()); return true; }
	}
	return false;
}

bool FTAInputDeviceDetector::HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event)
{
	if (Owner) Owner->RecordHeldInput(Event.GetKey(), 0.f, Event.GetUserIndex());
	return ConsumedPresses.Remove(Event.GetKey()) > 0;
}

bool FTAInputDeviceDetector::HandleAnalogInputEvent(FSlateApplication& SoftApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (!Owner)
	{
		return false;
	}

	const FKey Key = InAnalogInputEvent.GetKey();
	if (!Owner->OwnsPlayerInput())
	{
		Owner->RecordHeldInput(Key, 0.f, InAnalogInputEvent.GetUserIndex());
		return false;
	}
	Owner->NotifyRawInputKey(Key);
	Owner->RecordHeldInput(Key, InAnalogInputEvent.GetAnalogValue(), InAnalogInputEvent.GetUserIndex());
	if ((!Owner->IsUIInputModeActive() && !Owner->IsScanCursorModeActive()) || !Key.IsGamepadKey())
	{
		return false;
	}

	if (Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_LeftY ||
		(Owner->IsScanCursorModeActive() && (Key == EKeys::Gamepad_RightX || Key == EKeys::Gamepad_RightY)))
	{
		if (!Owner->GetLocalPlayer() || InAnalogInputEvent.GetUserIndex() != Owner->GetLocalPlayer()->GetControllerId()) return false;
		Owner->SetVirtualCursorAxis(Key, InAnalogInputEvent.GetAnalogValue());
		// Scan keeps Enhanced Input's axis state current; the character suppresses its scan Look callback.
		return Owner->IsUIInputModeActive();
	}
	return false;
}

void FTAInputDeviceDetector::Tick(const float DeltaTime, FSlateApplication& SoftApp, TSharedRef<ICursor> Cursor)
{
	if (Owner)
	{
		Owner->RefreshInputOwnership();
		Owner->TickVirtualCursor(DeltaTime);
	}
}

bool FTAInputDeviceDetector::HandleMouseButtonDownEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent)
{
	const FVector2D Position = MouseEvent.GetScreenSpacePosition();
	if (!Owner || !Owner->OwnsPlayerInput(&Position)) return false;
	if (Owner && Owner->IsSimulatingSyntheticLeftMouseClick())
	{
		return false;
	}
	if (Owner)
	{
		Owner->NotifyRawInputKey(MouseEvent.GetEffectingButton());
		Owner->RecordHeldInput(MouseEvent.GetEffectingButton(), 1.f, MouseEvent.GetUserIndex());
		if (Owner->HandlePuzzleConfirm(MouseEvent.GetEffectingButton(), false, MouseEvent.GetUserIndex()) ||
			Owner->HandlePuzzleUndo(MouseEvent.GetEffectingButton(), false, MouseEvent.GetUserIndex()))
		{ ConsumedPresses.Add(MouseEvent.GetEffectingButton()); return true; }
	}
	return false;
}

bool FTAInputDeviceDetector::HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent& Event)
{
	if (Owner && Owner->IsSimulatingSyntheticLeftMouseClick())
	{
		return false;
	}
	if (Owner) Owner->RecordHeldInput(Event.GetEffectingButton(), 0.f, Event.GetUserIndex());
	return ConsumedPresses.Remove(Event.GetEffectingButton()) > 0;
}

bool FTAInputDeviceDetector::HandleMouseMoveEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent)
{
	const FVector2D Position = MouseEvent.GetScreenSpacePosition();
	if (!Owner || !Owner->OwnsPlayerInput(&Position)) return false;
	if (Owner && MouseEvent.GetCursorDelta().SizeSquared() > 0.0f)
	{
		Owner->NotifyRawInputKey(EKeys::MouseX);
	}
	return false;
}

void AThe_AwakeningPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		InputDeviceDetector = MakeShared<FTAInputDeviceDetector>(this);
		DeviceConnectionHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this,
			[this](EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId)
			{
				if (State == EInputDeviceConnectionState::Disconnected && GetLocalPlayer() &&
					User == GetLocalPlayer()->GetPlatformUserId())
				{
					for (auto It = HeldKeyValues.CreateIterator(); It; ++It)
						if (It.Key().IsGamepadKey()) It.RemoveCurrent();
					for (auto It = ConsumedInventoryKeys.CreateIterator(); It; ++It)
						if (It->IsGamepadKey()) It.RemoveCurrent();
					VirtualCursorAxis = FVector2D::ZeroVector; ScanRightCursorAxis = FVector2D::ZeroVector;
				}
			});
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().RegisterInputPreProcessor(InputDeviceDetector);
			ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
				this, &AThe_AwakeningPlayerController::NotifyApplicationActivationChanged);
		}
	}
	if (IsLocalPlayerController() && FSlateApplication::IsInitialized())
	{
		TSharedRef<FNavigationConfig> NavigationConfig =
			FSlateApplication::Get().GetNavigationConfig();

		// Tab / Shift+Tab
		NavigationConfig->bTabNavigation = false;

		// 键盘方向键 / D-Pad
		NavigationConfig->bKeyNavigation = false;

		// 手柄摇杆
		NavigationConfig->bAnalogNavigation = false;
	}
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogThe_Awakening, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void AThe_AwakeningPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(DeviceConnectionHandle);
	if (InputDeviceDetector.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputDeviceDetector);
		InputDeviceDetector.Reset();
	}
	if (ApplicationActivationHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
		ApplicationActivationHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AThe_AwakeningPlayerController::NotifyRawInputKey(const FKey& Key)
{
	if (bSimulatingSyntheticLeftMouseClick && Key == EKeys::LeftMouseButton)
	{
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTAInputIconSubsystem* IconSys = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			IconSys->NotifyInputKey(Key);
		}
	}
}

void AThe_AwakeningPlayerController::SimulateSyntheticLeftMouseClick()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	TGuardValue<bool> SyntheticClickGuard(bSimulatingSyntheticLeftMouseClick, true);
	FSlateApplication& SlateApp = FSlateApplication::Get();
	const FVector2D CursorPosition = SlateApp.GetCursorPos();
	const TSet<FKey> PressedButtons = { EKeys::LeftMouseButton };
	const TSet<FKey> ReleasedButtons;
	const FModifierKeysState Modifiers;
	const FPointerEvent MouseDownEvent(
		0, 0, CursorPosition, CursorPosition, PressedButtons, EKeys::LeftMouseButton, 0.0f, Modifiers);
	const FPointerEvent MouseUpEvent(
		0, 0, CursorPosition, CursorPosition, ReleasedButtons, EKeys::LeftMouseButton, 0.0f, Modifiers);

	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();
	const TSharedPtr<FGenericWindow> NativeWindow = ActiveWindow.IsValid() ? ActiveWindow->GetNativeWindow() : nullptr;
	SlateApp.ProcessMouseButtonDownEvent(NativeWindow, MouseDownEvent);
	SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
}

void AThe_AwakeningPlayerController::RecordHeldInput(FKey Key, float Value, int32 UserIndex)
{
	if (!bApplicationInputActive || !GetLocalPlayer() || UserIndex != GetLocalPlayer()->GetControllerId()) return;
	// Releases always clear observed state. Press ownership is checked at ingress;
	// this observer never grants permission to execute a gameplay action.
	HeldKeyValues.FindOrAdd(Key) = FVector(Value, 0.f, 0.f);
	// Raw releases survive menu focus/input-mode changes and rearm the toggle.
	if (FMath::IsNearlyZero(Value)) ConsumedInventoryKeys.Remove(Key);
	// Slate sends separate X/Y events even when the mapping uses Gamepad_Left2D.
	const EPairedAxis Axis = Key.GetPairedAxis();
	if (Axis != EPairedAxis::Unpaired)
	{
		FVector& Pair = HeldKeyValues.FindOrAdd(Key.GetPairedAxisKey());
		if (Axis == EPairedAxis::X) Pair.X = Value;
		else if (Axis == EPairedAxis::Y) Pair.Y = Value;
		else if (Axis == EPairedAxis::Z) Pair.Z = Value;
	}
}

bool AThe_AwakeningPlayerController::ConsumeInventoryTogglePress(const UInputAction* Action)
{
	if (!ConsumedInventoryKeys.IsEmpty()) return false;
	const auto* Input = Cast<UTAPlayerInput>(PlayerInput);
	if (Input && Action)
		for (const auto& Mapping : Input->GetHeldActionMappings())
			if (Mapping.Action == Action && !HeldKeyValues.FindRef(Mapping.Key).IsNearlyZero())
				ConsumedInventoryKeys.Add(Mapping.Key);
	return true;
}

FInputActionValue AThe_AwakeningPlayerController::ReadHeldAction(const UInputAction* Action)
{
	if (!Action) return FInputActionValue();
	FInputActionValue Result(Action->ValueType, FVector::ZeroVector);
	const UTAPlayerInput* Enhanced = Cast<UTAPlayerInput>(PlayerInput);
	if (!Enhanced || !AllowsInput(ETAInputCapability::Gameplay)) return Result;
	// UI -> game focus may take a frame to settle. Suppress gameplay while unfocused,
	// but preserve held keys so a keyboard does not need a second key-down to resume.
	// Key-up events still update the cache; actual application deactivation clears it.
	auto Modify = [&](const TArray<TObjectPtr<UInputModifier>>& Modifiers, FInputActionValue Value)
	{
		for (UInputModifier* Source : Modifiers)
		{
			if (!Source) continue;
			// Separate instances: evaluating held intent must not advance Enhanced Input's smoothing state twice.
			TObjectPtr<UInputModifier>& Modifier = HeldInputModifiers.FindOrAdd(Source);
			if (!Modifier) Modifier = DuplicateObject<UInputModifier>(Source, this);
			Value = Modifier->ModifyRaw(Enhanced, Value, GetWorld()->GetDeltaSeconds());
			Value.ConvertToType(Action->ValueType);
		}
		return Value;
	};
	for (const FEnhancedActionKeyMapping& Mapping : Enhanced->GetHeldActionMappings())
	{
		if (Mapping.Action != Action) continue;
		const FInputActionValue Value = Modify(Mapping.Modifiers,
			FInputActionValue(Action->ValueType, HeldKeyValues.FindRef(Mapping.Key)));
		if (Action->AccumulationBehavior == EInputActionAccumulationBehavior::Cumulative) Result += Value;
		else
		{
			FVector Combined(Result[0], Result[1], Result[2]);
			for (int32 Axis = 0; Axis < 3; ++Axis)
				if (FMath::Abs(Value[Axis]) > FMath::Abs(Combined[Axis])) Combined[Axis] = Value[Axis];
			Result = FInputActionValue(Action->ValueType, Combined);
		}
	}
	// These actions deliberately represent held intent, not Pressed/Hold trigger timing.
	return Modify(Action->Modifiers, Result);
}

void AThe_AwakeningPlayerController::SetDialogueModeActive(bool bActive, UUserWidget* FocusWidget)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bDialogueModeActive == bActive)
	{
		return;
	}
	bDialogueModeActive = bActive;
	if (bActive)
	{
		BeginUIInputMode(FocusWidget);
		DialogueInputOwner = FocusWidget;
	}
	else
	{
		EndUIInputMode(DialogueInputOwner.Get());
		DialogueInputOwner.Reset();
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (bActive)
		{
			// 对话期间移除默认映射（移动/交互等），由对话 UI 的高优先级上下文接管
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->RemoveMappingContext(CurrentContext);
			}
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				Subsystem->RemoveMappingContext(CurrentContext);
			}
		}
		else
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AThe_AwakeningPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AThe_AwakeningPlayerController::BeginUIInputMode(UUserWidget* FocusWidget)
{
	InternalFocusTransitionUntil = GFrameCounter + 2;
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (ScanningComponent && ScanningComponent->IsScanning())
	{
		ScanningComponent->CancelScan(ETAScanEndReason::UIInterrupted, true);
	}

	++ActiveUIModeCount;
	MenuInputOwners.Add(FocusWidget);
	MenuInputHandles.Add(InputRouter.Acquire(FocusWidget ? static_cast<UObject*>(FocusWidget) : this, 200,
		{ETAInputCapability::Menu, ETAInputCapability::Cursor}));
	if (ActiveUIModeCount > 1)
	{
		if (FocusWidget) FocusWidget->SetUserFocus(this);
		return;
	}

	bUIInputModeActive = true;
	VirtualCursorAxis = FVector2D::ZeroVector; ScanRightCursorAxis = FVector2D::ZeroVector;
	if (AThe_AwakeningCharacter* ControlledCharacter = Cast<AThe_AwakeningCharacter>(GetPawn()))
	{
		ControlledCharacter->ClearMovementInput();
	}
	// Preserve the state from before scanning. Otherwise opening inventory during
	// a scan would remember the scan cursor and restore it after both modes end.
	bPreviousShowMouseCursor = bScanCursorModeActive ? bMouseCursorBeforeScan : bShowMouseCursor;
	SetShowMouseCursor(true);
	FInputModeGameAndUI InputMode;
	if (FocusWidget)
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetUIFocusWidget(FocusWidget);
}

void AThe_AwakeningPlayerController::EndUIInputMode(UUserWidget* RequestOwner)
{
	InternalFocusTransitionUntil = GFrameCounter + 2;
	if (!IsLocalPlayerController() || ActiveUIModeCount <= 0)
	{
		return;
	}

	const int32 Index = RequestOwner ? MenuInputOwners.IndexOfByPredicate([RequestOwner](const auto& Owner) { return Owner.Get() == RequestOwner; }) : MenuInputHandles.Num()-1;
	if (!MenuInputHandles.IsValidIndex(Index)) return;
	InputRouter.Release(MenuInputHandles[Index]);
	MenuInputHandles.RemoveAt(Index); MenuInputOwners.RemoveAt(Index);
	ActiveUIModeCount = MenuInputHandles.Num();
	if (ActiveUIModeCount > 0)
	{
		if (auto* Focus = MenuInputOwners.Last().Get()) Focus->SetUserFocus(this);
		return;
	}

	bUIInputModeActive = false;
	VirtualCursorAxis = FVector2D::ZeroVector; ScanRightCursorAxis = FVector2D::ZeroVector;
	SetShowMouseCursor(bScanCursorModeActive ? true : bPreviousShowMouseCursor);
	SetInputMode(FInputModeGameOnly());
}

void AThe_AwakeningPlayerController::NotifyApplicationActivationChanged(bool bIsActive)
{
	bApplicationInputActive = bIsActive;
	if (!bIsActive)
	{
		HeldKeyValues.Reset();
		ConsumedInventoryKeys.Reset();
		HeldInputModifiers.Reset();
		VirtualCursorAxis = FVector2D::ZeroVector; ScanRightCursorAxis = FVector2D::ZeroVector;
		if (auto* ControlledCharacter = Cast<AThe_AwakeningCharacter>(GetPawn())) ControlledCharacter->ClearMovementInput();
	}
	if (!ScanningComponent)
	{
		return;
	}
	if (!bIsActive)
	{
		ScanningComponent->CancelScan(ETAScanEndReason::Canceled, true);
	}
	else
	{
		// The release may have occurred while the application was unfocused.
		ScanningComponent->NotifyScanInputReleased(true);
	}
}

void AThe_AwakeningPlayerController::BeginScanCursorMode()
{
	if (!IsLocalPlayerController() || bScanCursorModeActive)
	{
		return;
	}

	bScanCursorModeActive = true;
	ScanInputHandle = InputRouter.Acquire(this, 100, {ETAInputCapability::Scan, ETAInputCapability::Look, ETAInputCapability::Cursor});
	SetVirtualCursorAxis(EKeys::Gamepad_LeftX, HeldKeyValues.FindRef(EKeys::Gamepad_Left2D).X);
	SetVirtualCursorAxis(EKeys::Gamepad_LeftY, HeldKeyValues.FindRef(EKeys::Gamepad_Left2D).Y);
	SetVirtualCursorAxis(EKeys::Gamepad_RightX, HeldKeyValues.FindRef(EKeys::Gamepad_Right2D).X);
	SetVirtualCursorAxis(EKeys::Gamepad_RightY, HeldKeyValues.FindRef(EKeys::Gamepad_Right2D).Y);
	bMouseCursorBeforeScan = bUIInputModeActive ? bPreviousShowMouseCursor : bShowMouseCursor;
	SetShowMouseCursor(true);
}

void AThe_AwakeningPlayerController::EndScanCursorMode()
{
	if (!IsLocalPlayerController() || !bScanCursorModeActive)
	{
		return;
	}

	bScanCursorModeActive = false;
	InputRouter.Release(ScanInputHandle); ScanInputHandle = 0;
	VirtualCursorAxis = FVector2D::ZeroVector; ScanRightCursorAxis = FVector2D::ZeroVector;
	if (bUIInputModeActive)
	{
		SetShowMouseCursor(true);
		return;
	}

	SetShowMouseCursor(bMouseCursorBeforeScan);
	SetInputMode(FInputModeGameOnly());
}

void AThe_AwakeningPlayerController::SetUIFocusWidget(UUserWidget* FocusWidget)
{
	if (!bUIInputModeActive || !IsValid(FocusWidget))
	{
		return;
	}

	UButton* FirstFocusableButton = nullptr;
	if (FocusWidget->WidgetTree)
	{
		FocusWidget->WidgetTree->ForEachWidgetAndDescendants([&FirstFocusableButton](UWidget* Widget)
		{
			UButton* Button = Cast<UButton>(Widget);
			if (!FirstFocusableButton && Button && Button->GetIsFocusable() &&
				Button->GetVisibility() == ESlateVisibility::Visible && Button->GetIsEnabled())
			{
				FirstFocusableButton = Button;
			}
		});
	}

	if (FirstFocusableButton)
	{
		FirstFocusableButton->SetUserFocus(this);
	}
	else
	{
		FocusWidget->SetUserFocus(this);
	}
}

void AThe_AwakeningPlayerController::SetVirtualCursorAxis(const FKey& AxisKey, float Value)
{
	constexpr float DeadZone = 0.18f;
	const float Magnitude = FMath::Abs(Value);
	const float Remapped = Magnitude <= DeadZone ? 0.0f : FMath::Sign(Value) * ((Magnitude - DeadZone) / (1.0f - DeadZone));
	if (AxisKey == EKeys::Gamepad_LeftX)
	{
		VirtualCursorAxis.X = Remapped;
	}
	else if (AxisKey == EKeys::Gamepad_LeftY)
	{
		VirtualCursorAxis.Y = Remapped;
	}
	else if (AxisKey == EKeys::Gamepad_RightX) ScanRightCursorAxis.X = Remapped;
	else if (AxisKey == EKeys::Gamepad_RightY) ScanRightCursorAxis.Y = Remapped;
}

void AThe_AwakeningPlayerController::TickVirtualCursor(float DeltaTime)
{
	if (!AllowsInput(ETAInputCapability::Cursor)) return;
	if (PuzzleScopeWidget.IsValid()) return;
	const FVector2D CursorAxis = (VirtualCursorAxis + (bScanCursorModeActive && !bUIInputModeActive ? ScanRightCursorAxis : FVector2D::ZeroVector)).GetClampedToMaxSize(1.f);
	if ((!bUIInputModeActive && !bScanCursorModeActive) || !bApplicationInputActive || CursorAxis.IsNearlyZero() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	if (!ViewportWidget.IsValid())
	{
		return;
	}

	const FGeometry Geometry = ViewportWidget->GetCachedGeometry();
	const FVector2D ViewportOrigin(Geometry.GetAbsolutePosition());
	const FVector2D ViewportSize(Geometry.GetAbsoluteSize());
	if (ViewportSize.X <= 1.0f || ViewportSize.Y <= 1.0f)
	{
		return;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FVector2D CursorPosition(SlateApp.GetCursorPos());
	AThe_AwakeningCharacter* PlayerCharacter = Cast<AThe_AwakeningCharacter>(GetPawn());
	const float CursorSpeed = PlayerCharacter ? PlayerCharacter->GetMenuCursorSpeed() : 1100.0f;
	CursorPosition += FVector2D(CursorAxis.X, -CursorAxis.Y) * CursorSpeed * DeltaTime;
	CursorPosition.X = FMath::Clamp(CursorPosition.X, ViewportOrigin.X, ViewportOrigin.X + ViewportSize.X - 1.0f);
	CursorPosition.Y = FMath::Clamp(CursorPosition.Y, ViewportOrigin.Y, ViewportOrigin.Y + ViewportSize.Y - 1.0f);
	SlateApp.SetCursorPos(CursorPosition);
	// Slate time keeps both controls responsive while the character is frozen.
	// The character suppresses the normal gamepad Look callback during scanning.
	if (IsScanStickCursorActive() && PlayerCharacter)
	{
		PlayerCharacter->DoLook(CursorAxis.X * DeltaTime * 60.f, -CursorAxis.Y * DeltaTime * 60.f);
	}
}

bool AThe_AwakeningPlayerController::GetScanCursorPosition(float& X, float& Y) const
{
	// SetCursorPos can move the OS cursor without updating SceneViewport's cached mouse position.
	// Read the same Slate position used to draw/move it, converting desktop units to viewport pixels.
	UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SViewport> Widget = Client ? Client->GetGameViewportWidget() : nullptr;
	if (FSlateApplication::IsInitialized() && Widget.IsValid())
	{
		const FGeometry Geometry = Widget->GetCachedGeometry();
		const FVector2D Size = Geometry.GetLocalSize();
		int32 Width = 0, Height = 0;
		GetViewportSize(Width, Height);
		if (Size.X > 0.f && Size.Y > 0.f && Width > 0 && Height > 0)
		{
			const FVector2D Local = Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
			X = Local.X * Width / Size.X;
			Y = Local.Y * Height / Size.Y;
			return true;
		}
	}
	return GetMousePosition(X, Y);
}

bool AThe_AwakeningPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (!OwnsPlayerInput() && Params.Event != IE_Released) return false;
	if (Params.Key.IsValid())
	{
		NotifyRawInputKey(Params.Key);
	}
	return Super::InputKey(Params);
}

void AThe_AwakeningPlayerController::SetPuzzleScopeWidget(UTAPathPuzzleWidget* Widget)
{
	InputRouter.Release(PuzzleInputHandle); PuzzleInputHandle = 0;
	if (Widget) PuzzleInputHandle = InputRouter.Acquire(Widget, 300, {ETAInputCapability::Puzzle});
    PuzzleScopeWidget = Widget;
    VirtualCursorAxis = ScanRightCursorAxis = FVector2D::ZeroVector;
    if (Widget)
    {
        SetShowMouseCursor(false);
        Widget->SetUserFocus(this);
    }
    else if (bUIInputModeActive) SetShowMouseCursor(true);
}

FVector2D AThe_AwakeningPlayerController::GetPuzzlePanInput() const
{
	if (!AllowsInput(ETAInputCapability::Puzzle)) return FVector2D::ZeroVector;
    if (!bApplicationInputActive || !PuzzleScopeWidget.IsValid()) return FVector2D::ZeroVector;
    auto Down = [this](FKey Key) { return HeldKeyValues.FindRef(Key).X != 0.f ? 1.f : 0.f; };
	FVector2D Keyboard(Down(EKeys::A) - Down(EKeys::D), Down(EKeys::W) - Down(EKeys::S));
	if (!PuzzleScopeWidget->bInversePanInput) Keyboard *= -1.f;
    if (!Keyboard.IsNearlyZero()) return Keyboard.GetClampedToMaxSize(1.f);
    const FVector Stick = HeldKeyValues.FindRef(EKeys::Gamepad_Left2D);
    FVector2D Axis(-Stick.X, Stick.Y);
	if (!PuzzleScopeWidget->bInversePanInput) Axis *= -1.f;
    const float Magnitude = Axis.Size();
    return Magnitude > .18f ? Axis.GetSafeNormal() * FMath::Clamp((Magnitude - .18f) / .82f, 0.f, 1.f) : FVector2D::ZeroVector;
}

bool AThe_AwakeningPlayerController::HandlePuzzleConfirm(FKey Key, bool bRepeat, int32 UserIndex)
{
	if (!AllowsInput(ETAInputCapability::Puzzle)) return false;
    if (!bApplicationInputActive || !PuzzleScopeWidget.IsValid() || !GetLocalPlayer() ||
        UserIndex != GetLocalPlayer()->GetControllerId() ||
        (Key != EKeys::LeftMouseButton && Key != EKeys::Gamepad_FaceButton_Bottom)) return false;
    if (!bRepeat) PuzzleScopeWidget->ConfirmScopeNode();
	return true;
}

bool AThe_AwakeningPlayerController::HandlePuzzleUndo(FKey Key, bool bRepeat, int32 UserIndex)
{
	if (!AllowsInput(ETAInputCapability::Puzzle)) return false;
	if (!bApplicationInputActive || !PuzzleScopeWidget.IsValid() || !GetLocalPlayer() ||
		UserIndex != GetLocalPlayer()->GetControllerId() ||
		(Key != EKeys::RightMouseButton && Key != EKeys::Gamepad_FaceButton_Right)) return false;
	if (!bRepeat) PuzzleScopeWidget->Undo();
	return true;
}

bool AThe_AwakeningPlayerController::OwnsPlayerInput(const FVector2D* Pointer) const
{
	return bApplicationInputActive && TAInputOwnershipAdapter::OwnsInput(this, Pointer);
}

bool AThe_AwakeningPlayerController::AllowsInput(ETAInputCapability Capability) const
{
	return OwnsPlayerInput() && InputRouter.Allows(Capability);
}

void AThe_AwakeningPlayerController::RefreshInputOwnership()
{
	if (OwnsPlayerInput()) return;
	// SetInputMode transfers Slate focus asynchronously. Preserve physical state
	// briefly across our own transfer; permission stays denied until focus arrives.
	if (bApplicationInputActive && GFrameCounter <= InternalFocusTransitionUntil) return;
	HeldKeyValues.Reset(); HeldInputModifiers.Reset(); ConsumedInventoryKeys.Reset();
	VirtualCursorAxis = ScanRightCursorAxis = FVector2D::ZeroVector;
	if (auto* ControlledPawn = Cast<AThe_AwakeningCharacter>(GetPawn())) ControlledPawn->ClearMovementInput();
}
