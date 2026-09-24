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

void AThe_AwakeningPlayerController::SetDialogueModeActive(bool bActive)
{
	if (!IsLocalPlayerController())
	{
		return;
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

bool AThe_AwakeningPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key.IsValid())
	{
		NotifyRawInputKey(Params.Key);
	}
	return Super::InputKey(Params);
}
