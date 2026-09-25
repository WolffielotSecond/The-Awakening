// Copyright Epic Games, Inc. All Rights Reserved.

#include "The_AwakeningPlayerController.h"
#include "Scan/TAScanningComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
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


AThe_AwakeningPlayerController::AThe_AwakeningPlayerController()
{
	ScanningComponent = CreateDefaultSubobject<UTAScanningComponent>(TEXT("ScanComponent"));
}

bool FTAInputDeviceDetector::HandleKeyDownEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent)
{
	if (Owner)
	{
		Owner->NotifyRawInputKey(InKeyEvent.GetKey());
	}
	return false;
}

bool FTAInputDeviceDetector::HandleKeyUpEvent(FSlateApplication&, const FKeyEvent&)
{
	return false;
}

bool FTAInputDeviceDetector::HandleAnalogInputEvent(FSlateApplication& SoftApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (!Owner)
	{
		return false;
	}

	const FKey Key = InAnalogInputEvent.GetKey();
	Owner->NotifyRawInputKey(Key);
	if (!Owner->IsUIInputModeActive() || !Key.IsGamepadKey())
	{
		return false;
	}

	if (Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_LeftY)
	{
		Owner->SetVirtualCursorAxis(Key, InAnalogInputEvent.GetAnalogValue());
		return true;
	}
	return false;
}

void FTAInputDeviceDetector::Tick(const float DeltaTime, FSlateApplication& SoftApp, TSharedRef<ICursor> Cursor)
{
	if (Owner)
	{
		Owner->TickVirtualCursor(DeltaTime);
	}
}

bool FTAInputDeviceDetector::HandleMouseButtonDownEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent)
{
	if (Owner)
	{
		Owner->NotifyRawInputKey(MouseEvent.GetEffectingButton());
	}
	return false;
}

bool FTAInputDeviceDetector::HandleMouseMoveEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent)
{
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
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().RegisterInputPreProcessor(InputDeviceDetector);
			ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
				this, &AThe_AwakeningPlayerController::NotifyApplicationActivationChanged);
		}
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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTAInputIconSubsystem* IconSys = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			IconSys->NotifyInputKey(Key);
		}
	}
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
	}
	else
	{
		EndUIInputMode();
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
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (ScanningComponent && ScanningComponent->IsScanning())
	{
		ScanningComponent->CancelScan(ETAScanEndReason::UIInterrupted, true);
	}

	++ActiveUIModeCount;
	if (ActiveUIModeCount > 1)
	{
		return;
	}

	bUIInputModeActive = true;
	VirtualCursorAxis = FVector2D::ZeroVector;
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

void AThe_AwakeningPlayerController::EndUIInputMode()
{
	if (!IsLocalPlayerController() || ActiveUIModeCount <= 0)
	{
		return;
	}

	--ActiveUIModeCount;
	if (ActiveUIModeCount > 0)
	{
		return;
	}

	bUIInputModeActive = false;
	VirtualCursorAxis = FVector2D::ZeroVector;
	SetShowMouseCursor(bScanCursorModeActive ? true : bPreviousShowMouseCursor);
	SetInputMode(FInputModeGameOnly());
}

void AThe_AwakeningPlayerController::NotifyApplicationActivationChanged(bool bIsActive)
{
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
	if (bUIInputModeActive)
	{
		SetShowMouseCursor(true);
		return;
	}

	SetShowMouseCursor(bMouseCursorBeforeScan);
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
}

void AThe_AwakeningPlayerController::TickVirtualCursor(float DeltaTime)
{
	if (!bUIInputModeActive || VirtualCursorAxis.IsNearlyZero() || !FSlateApplication::IsInitialized())
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
	const AThe_AwakeningCharacter* PlayerCharacter = Cast<AThe_AwakeningCharacter>(GetPawn());
	const float CursorSpeed = PlayerCharacter ? PlayerCharacter->GetMenuCursorSpeed() : 1100.0f;
	CursorPosition += FVector2D(VirtualCursorAxis.X, -VirtualCursorAxis.Y) * CursorSpeed * DeltaTime;
	CursorPosition.X = FMath::Clamp(CursorPosition.X, ViewportOrigin.X, ViewportOrigin.X + ViewportSize.X - 1.0f);
	CursorPosition.Y = FMath::Clamp(CursorPosition.Y, ViewportOrigin.Y, ViewportOrigin.Y + ViewportSize.Y - 1.0f);
	SlateApp.SetCursorPos(CursorPosition);
}

bool AThe_AwakeningPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key.IsValid())
	{
		NotifyRawInputKey(Params.Key);
	}
	return Super::InputKey(Params);
}
