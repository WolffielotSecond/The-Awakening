// Copyright Epic Games, Inc. All Rights Reserved.

#include "The_AwakeningPlayerController.h"
#include "Scan/TAScanningComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "EnhancedPlayerInput.h"
#include "Core/TAPlayerInput.h"
#include "Core/TAInputOwnershipAdapter.h"
#include "Core/TAPlayerInputReceiver.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Blueprint/UserWidget.h"
#include "The_Awakening.h"
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
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateUser.h"


AThe_AwakeningPlayerController::AThe_AwakeningPlayerController()
{
	OverridePlayerInputClass = UTAPlayerInput::StaticClass();
	ScanningComponent = CreateDefaultSubobject<UTAScanningComponent>(TEXT("ScanComponent"));
}

bool FTAInputDeviceDetector::HandleKeyDownEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent)
{
	if (!Owner || !Owner->OwnsPlayerInput())
	{
		return false;
	}
	if (Owner)
	{
		Owner->NotifyRawInputKey(InKeyEvent.GetKey());
		Owner->RecordHeldInput(InKeyEvent.GetKey(), 1.f, InKeyEvent.GetUserIndex());
		if (Owner->RoutePlayerInputKey(InKeyEvent.GetKey(), InKeyEvent.IsRepeat(), InKeyEvent.GetUserIndex()))
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
	if (!Owner->ObserveAnalogInput(Key, InAnalogInputEvent.GetAnalogValue(), InAnalogInputEvent.GetUserIndex(),
		Owner->GetPlayerInputOwnershipState())) return false;
	if ((!Owner->AllowsInput(ETAInputCapability::Cursor) && !Owner->AllowsInput(ETAInputCapability::Pan)) || !Key.IsGamepadKey())
	{
		return false;
	}

	if (Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_LeftY ||
		(Owner->UsesCursorLook() && (Key == EKeys::Gamepad_RightX || Key == EKeys::Gamepad_RightY)))
	{
		if (!Owner->GetLocalPlayer() || InAnalogInputEvent.GetUserIndex() != Owner->GetLocalPlayer()->GetControllerId()) return false;
		// Scan keeps Enhanced Input's axis state current; the character suppresses its scan Look callback.
		return !Owner->AllowsInput(ETAInputCapability::Look);
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
		if (Owner->RoutePlayerInputKey(MouseEvent.GetEffectingButton(), false, MouseEvent.GetUserIndex()))
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
					InvalidateGamepadObservationOnDisconnect();
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

bool AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(
	const FWidgetPath& Path, const TSharedPtr<SViewport>& Surface)
{
	return Surface.IsValid() && Path.IsValid() && Path.ContainsWidget(Surface.Get());
}

void AThe_AwakeningPlayerController::SimulateSyntheticLeftMouseClick()
{
	if (!FSlateApplication::IsInitialized() || bSimulatingSyntheticLeftMouseClick ||
		!AllowsInput(ETAInputCapability::Confirm))
	{
		return;
	}
	FSlateApplication& SlateApp = FSlateApplication::Get();
	const FVector2D CursorPosition = SlateApp.GetCursorPos();
	const auto* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const auto Surface = Client ? Client->GetGameViewportWidget() : nullptr;
	const int32 User = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	const auto HitPath = SlateApp.LocateWindowUnderMouse(CursorPosition,
		SlateApp.GetInteractiveTopLevelWindows(), false, User);
	// The host window can also contain unrelated UI (including editor toolbars).
	// Validate Slate's target, not the rectangular cursor bounds or window identity.
	if (!IsSyntheticClickPathWithinPlayerSurface(HitPath, Surface)) return;
	const auto SlateUser = SlateApp.GetUser(User);
	if (SlateUser && SlateUser->HasCapture(0))
	{
		// ProcessMouseButtonDownEvent routes to an existing captor instead of HitPath.
		const auto CapturePath = SlateUser->GetCaptorPath(0, FWeakWidgetPath::EInterruptedPathHandling::Truncate);
		if (!IsSyntheticClickPathWithinPlayerSurface(CapturePath, Surface)) return;
	}
	TGuardValue<bool> SyntheticClickGuard(bSimulatingSyntheticLeftMouseClick, true);
	const TSet<FKey> PressedButtons = { EKeys::LeftMouseButton };
	const TSet<FKey> ReleasedButtons;
	const FModifierKeysState Modifiers;
	const FPointerEvent MouseDownEvent(
		User, 0, CursorPosition, CursorPosition, PressedButtons, EKeys::LeftMouseButton, 0.0f, Modifiers);
	const FPointerEvent MouseUpEvent(
		User, 0, CursorPosition, CursorPosition, ReleasedButtons, EKeys::LeftMouseButton, 0.0f, Modifiers);

	const auto NativeWindow = HitPath.GetWindow()->GetNativeWindow();
	SlateApp.ProcessMouseButtonDownEvent(NativeWindow, MouseDownEvent);
	// Always finish an emitted Down, even if its handler closes the originating UI.
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

TOptional<FVector> AThe_AwakeningPlayerController::FindHeldKeyObservation(FKey Key) const
{
	if (const FVector* Value = HeldKeyValues.Find(Key)) return *Value;
	return {};
}

TArray<FKey> AThe_AwakeningPlayerController::GetObservedHeldKeysForAction(const UInputAction* Action) const
{
	TArray<FKey> Sources;
	const auto* Input = Cast<UTAPlayerInput>(PlayerInput);
	if (Input && Action)
		for (const auto& Mapping : Input->GetHeldActionMappings())
		{
			const auto Value = FindHeldKeyObservation(Mapping.Key);
			if (Mapping.Action == Action && Value.IsSet() && !Value.GetValue().IsNearlyZero())
				Sources.AddUnique(Mapping.Key);
		}
	return Sources;
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
	if (!Enhanced) return Result;
	// Read physical observation only; consumers apply ownership/capability permission.
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

			// The legacy property contains desktop mouse mappings, not touch controls.
			// Keep its serialized name so existing Blueprint defaults remain valid.
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}
}

void AThe_AwakeningPlayerController::NotifyApplicationActivationChanged(bool bIsActive)
{
	bApplicationInputActive = bIsActive;
	// Activation can precede Slate's updated focus state. Inactivity is definitive;
	// activation only requests a fresh observation, never synthesizes a key release.
	ObservePlayerInputOwnership(GetPlayerInputOwnershipState());
}

TAInputOwnershipAdapter::EState AThe_AwakeningPlayerController::GetPlayerInputOwnershipState() const
{
	return bApplicationInputActive ? TAInputOwnershipAdapter::GetState(this) : TAInputOwnershipAdapter::EState::Lost;
}

bool AThe_AwakeningPlayerController::ObserveAnalogInput(FKey Key, float Value, int32 UserIndex,
	TAInputOwnershipAdapter::EState State)
{
	ObservePlayerInputOwnership(State);
	if (State != TAInputOwnershipAdapter::EState::Owned) return false;
	NotifyRawInputKey(Key);
	RecordHeldInput(Key, Value, UserIndex);
	return true;
}

void AThe_AwakeningPlayerController::InvalidatePhysicalObservationForExternalOwnership()
{
	HeldKeyValues.Reset();
	HeldInputModifiers.Reset();
	ConsumedInventoryKeys.Reset();
	// Preserve the existing external-loss cleanup; permission denial never calls this.
	if (auto* ControlledCharacter = Cast<AThe_AwakeningCharacter>(GetPawn())) ControlledCharacter->ClearMovementInput();
	// Do not generate Released, change requests, or clear the processor's Down/Up pairing.
}

void AThe_AwakeningPlayerController::InvalidateGamepadObservationOnDisconnect()
{
	// The caller retains the existing local platform-user/disconnected filtering.
	// Held values are keyed by FKey, not device ID: this is the existing gamepad scope.
	for (auto It = HeldKeyValues.CreateIterator(); It; ++It)
		if (It.Key().IsGamepadKey()) It.RemoveCurrent();
	for (auto It = ConsumedInventoryKeys.CreateIterator(); It; ++It)
		if (It->IsGamepadKey()) It.RemoveCurrent();
}

FVector2D AThe_AwakeningPlayerController::ReadCursorStick(FKey PairedAxisKey) const
{
	const FVector Observed = ReadHeldKey(PairedAxisKey);
	auto Remap = [](float Value)
	{
		constexpr float DeadZone = 0.18f;
		const float Magnitude = FMath::Abs(Value);
		return Magnitude <= DeadZone ? 0.0f : FMath::Sign(Value) * ((Magnitude - DeadZone) / (1.0f - DeadZone));
	};
	return FVector2D(Remap(Observed.X), Remap(Observed.Y));
}

FVector2D AThe_AwakeningPlayerController::GetCursorInputAxis() const
{
	return (ReadCursorStick(EKeys::Gamepad_Left2D) +
		(UsesCursorLook() ? ReadCursorStick(EKeys::Gamepad_Right2D) : FVector2D::ZeroVector)).GetClampedToMaxSize(1.f);
}

bool AThe_AwakeningPlayerController::IsCursorStickLookActive() const
{
	// Preserve warp suppression even when the two deflected sticks cancel each other.
	return UsesCursorLook() && (!ReadCursorStick(EKeys::Gamepad_Left2D).IsNearlyZero() ||
		!ReadCursorStick(EKeys::Gamepad_Right2D).IsNearlyZero());
}

void AThe_AwakeningPlayerController::TickVirtualCursor(float DeltaTime)
{
	if (!AllowsInput(ETAInputCapability::Cursor)) return;
	const FVector2D CursorAxis = GetCursorInputAxis();
	if (!bApplicationInputActive || CursorAxis.IsNearlyZero() || !FSlateApplication::IsInitialized())
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
	if (IsCursorStickLookActive() && PlayerCharacter)
	{
		PlayerCharacter->SubmitPlayerLook(CursorAxis.X * DeltaTime * 60.f, -CursorAxis.Y * DeltaTime * 60.f);
	}
}

bool AThe_AwakeningPlayerController::GetPlayerCursorPosition(float& X, float& Y) const
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

bool AThe_AwakeningPlayerController::RoutePlayerInputKey(FKey Key, bool bRepeat, int32 UserIndex)
{
	if (!GetLocalPlayer() || UserIndex != GetLocalPlayer()->GetControllerId()) return false;
	const auto Winner = GetInputWinner();
	auto* Receiver = Cast<ITAPlayerInputReceiver>(Winner.Request.Owner.Get());
	if (!Receiver) return false;
	const auto Action = Receiver->ResolvePlayerInput(Key);
	if (!Action.IsSet() || !AllowsInputFor(Receiver->GetPlayerInputRequestHandle(),
		Winner.Request.Owner.Get(), Action.GetValue())) return false;
	// Consume repeats and matching releases without executing a second command.
	if (!bRepeat) Receiver->ExecutePlayerInput(Key, Action.GetValue());
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
	ObservePlayerInputOwnership(GetPlayerInputOwnershipState());
	SynchronizeInputPresentation();
}

bool AThe_AwakeningPlayerController::AllowsInputFor(FTAInputRouter::FHandle Handle, const UObject* RequestOwner, ETAInputCapability Capability) const
{
	return OwnsPlayerInput() && InputRouter.AllowsFor(Handle, RequestOwner, Capability);
}

void AThe_AwakeningPlayerController::ObservePlayerInputOwnership(TAInputOwnershipAdapter::EState State)
{
	using TAInputOwnershipAdapter::EState;
	if (State == EState::Transition) return; // Unknown focus gap is neither a grant nor a loss.
	const bool bLost = State == EState::Lost && LastDefinitiveInputOwnership == EState::Owned;
	const bool bInvalidate = State == EState::Lost && LastDefinitiveInputOwnership != EState::Lost;
	LastDefinitiveInputOwnership = State; // Commit before callbacks can release requests/re-enter.
	// Initial Lost also invalidates, but only an Owned -> Lost edge cancels held behaviors.
	if (bInvalidate) InvalidatePhysicalObservationForExternalOwnership();
	if (bLost) OnPlayerInputOwnershipLost.Broadcast();
}

FTAInputRouter::FHandle AThe_AwakeningPlayerController::AcquireInputRequest(const FTAInputRequest& Request)
{
	if (!IsLocalPlayerController()) return 0;
	ObservePlayerInputOwnership(GetPlayerInputOwnershipState());
	const auto Handle = InputRouter.Acquire(Request);
	SynchronizeInputPresentation();
	return Handle;
}

void AThe_AwakeningPlayerController::ReleaseInputRequest(FTAInputRouter::FHandle Handle)
{
	InputRouter.Release(Handle);
	SynchronizeInputPresentation();
}

FTAInputRouter::FWinner AThe_AwakeningPlayerController::GetInputWinner() const
{
	return InputRouter.GetWinner();
}

bool AThe_AwakeningPlayerController::UsesCursorLook() const
{
	const auto Winner = GetInputWinner();
	return Winner.Request.Allowed.Contains(ETAInputCapability::Look) && Winner.Request.Allowed.Contains(ETAInputCapability::Cursor);
}

void AThe_AwakeningPlayerController::SynchronizeInputPresentation()
{
	if (!IsLocalPlayerController()) return;
	auto Winner = GetInputWinner();
	if (!ObservedWinnerHandle.IsSet() || ObservedWinnerHandle.GetValue() != Winner.Handle)
	{
		ObservedWinnerHandle = Winner.Handle;
		OnInputOwnerChanged.Broadcast();
		// A listener may release its request. Never apply a stale winner after callbacks.
		Winner = GetInputWinner();
	}
	const auto& Presentation = Winner.Request.Presentation;
	UWidget* CursorOwner = Cast<UWidget>(Winner.Request.Owner.Get());
	if (PresentedCursorOwner != CursorOwner || bShowMouseCursor != Presentation.bShowCursor)
	{
		if (PresentedCursorOwner != CursorOwner)
			if (auto* Old = PresentedCursorOwner.Get()) Old->ResetCursor();
		PresentedCursorOwner = CursorOwner;
		SetShowMouseCursor(Presentation.bShowCursor);
		// Slate queries UMG before the controller; both use the same winning policy.
		if (CursorOwner) CursorOwner->SetCursor(Presentation.bShowCursor ? EMouseCursor::Default : EMouseCursor::None);
	}

	auto* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const auto Viewport = Client ? Client->GetGameViewportWidget() : TSharedPtr<SViewport>();
	UWidget* Focus = Presentation.Focus == ETAInputFocusRequirement::Target ? Presentation.FocusTarget.Get() : nullptr;
	// A live UObject without a usable Slate surface also falls back to the viewport.
	if (Focus && (!Focus->GetCachedWidget().IsValid() || !Focus->GetIsEnabled() || !Focus->IsVisible())) Focus = nullptr;
	if (PresentedRequestHandle.IsSet() && PresentedRequestHandle.GetValue() == Winner.Handle &&
		PresentedFocusTarget == Focus && PresentedViewport.Pin() == Viewport) return;
	if (!Viewport.IsValid() || !TAInputOwnershipAdapter::CanApplyPresentation(this)) return;
	const TSharedPtr<SWidget> FocusWidget = Focus ? Focus->GetCachedWidget() : Viewport;
	switch (Presentation.InputMode)
	{
	case ETAInputModeRequirement::GameOnly:
		SetInputMode(FInputModeGameOnly());
		break;
	case ETAInputModeRequirement::GameAndUI:
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(FocusWidget).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock).SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		break;
	}
	case ETAInputModeRequirement::UIOnly:
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(FocusWidget).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		break;
	}
	}
	PresentedRequestHandle = Winner.Handle;
	PresentedFocusTarget = Focus;
	PresentedViewport = Viewport;
}

bool AThe_AwakeningPlayerController::IsKeyMappedToAction(FKey Key, const UInputAction* Action) const
{
	const auto* Input = Cast<UTAPlayerInput>(PlayerInput);
	if (!Input || !Action) return false;
	for (const auto& Mapping : Input->GetHeldActionMappings())
		if (Mapping.Action == Action && Mapping.Key == Key) return true;
	return false;
}
