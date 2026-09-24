#include "Core/TAInputIconSubsystem.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputDeviceSubsystem.h"

void UTAInputIconSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentDeviceType = EInputDeviceType::KeyboardMouse;
}

void UTAInputIconSubsystem::SetCurrentDeviceType(EInputDeviceType NewType)
{
	if (CurrentDeviceType != NewType)
	{
		CurrentDeviceType = NewType;
		OnInputDeviceChanged.Broadcast();
	}
}

void UTAInputIconSubsystem::NotifyInputKey(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return;
	}

	EInputDeviceType NewType = CurrentDeviceType;

	if (Key.IsGamepadKey())
	{
		NewType = EInputDeviceType::Xbox;
	}
	else
	{
		NewType = EInputDeviceType::KeyboardMouse;
	}

	if (NewType != CurrentDeviceType)
	{
		CurrentDeviceType = NewType;
		OnInputDeviceChanged.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("Input device changed"));
	}
}

FString UTAInputIconSubsystem::KeyToAssetName(FKey Key) const
{
	if (!Key.IsValid())
	{
		return FString();
	}

	// 鼠标（Build 时加 Mouse_ 前缀）
	if (Key == EKeys::LeftMouseButton)   return TEXT("Key_1_Left");
	if (Key == EKeys::RightMouseButton)  return TEXT("Key_2_Right");
	if (Key == EKeys::MiddleMouseButton) return TEXT("Key_3");
	if (Key == EKeys::ThumbMouseButton)  return TEXT("Key_4");
	if (Key == EKeys::ThumbMouseButton2) return TEXT("Key_5");

	// 键盘特殊
	if (Key == EKeys::SpaceBar) return TEXT("Space");
	if (Key == EKeys::Escape) return TEXT("Esc");
	if (Key == EKeys::Enter) return TEXT("Enter");
	if (Key == EKeys::BackSpace) return TEXT("BackSpace");
	if (Key == EKeys::Tab) return TEXT("Tab");
	if (Key == EKeys::CapsLock) return TEXT("CapsLock");
	if (Key == EKeys::LeftShift) return TEXT("Shift_L");
	if (Key == EKeys::RightShift) return TEXT("Shift_R");
	if (Key == EKeys::LeftControl) return TEXT("Ctrl_L");
	if (Key == EKeys::RightControl) return TEXT("Ctrl_R");
	if (Key == EKeys::LeftAlt) return TEXT("Alt_L");
	if (Key == EKeys::RightAlt) return TEXT("Alt_R");
	if (Key == EKeys::LeftCommand || Key == EKeys::RightCommand) return TEXT("macOS_Command");

	if (Key == EKeys::Up) return TEXT("Arrow_Up");
	if (Key == EKeys::Down) return TEXT("Arrow_Down");
	if (Key == EKeys::Left) return TEXT("Arrow_Left");
	if (Key == EKeys::Right) return TEXT("Arrow_Right");

	if (Key == EKeys::PageUp) return TEXT("PageUp");
	if (Key == EKeys::PageDown) return TEXT("PageDown");
	if (Key == EKeys::Home) return TEXT("Home");
	if (Key == EKeys::End) return TEXT("End");
	if (Key == EKeys::Insert) return TEXT("Insert");
	if (Key == EKeys::Delete) return TEXT("Delete");

	if (Key == EKeys::F1) return TEXT("F1");
	if (Key == EKeys::F2) return TEXT("F2");
	if (Key == EKeys::F3) return TEXT("F3");
	if (Key == EKeys::F4) return TEXT("F4");
	if (Key == EKeys::F5) return TEXT("F5");
	if (Key == EKeys::F6) return TEXT("F6");
	if (Key == EKeys::F7) return TEXT("F7");
	if (Key == EKeys::F8) return TEXT("F8");
	if (Key == EKeys::F9) return TEXT("F9");
	if (Key == EKeys::F10) return TEXT("F10");
	if (Key == EKeys::F11) return TEXT("F11");
	if (Key == EKeys::F12) return TEXT("F12");

	if (Key == EKeys::Zero) return TEXT("0");
	if (Key == EKeys::One) return TEXT("1");
	if (Key == EKeys::Two) return TEXT("2");
	if (Key == EKeys::Three) return TEXT("3");
	if (Key == EKeys::Four) return TEXT("4");
	if (Key == EKeys::Five) return TEXT("5");
	if (Key == EKeys::Six) return TEXT("6");
	if (Key == EKeys::Seven) return TEXT("7");
	if (Key == EKeys::Eight) return TEXT("8");
	if (Key == EKeys::Nine) return TEXT("9");

	// 手柄逻辑名（再映射到 Xbox/PS 资源名）
	if (Key == EKeys::Gamepad_FaceButton_Bottom) return TEXT("Face_Bottom");
	if (Key == EKeys::Gamepad_FaceButton_Right)  return TEXT("Face_Right");
	if (Key == EKeys::Gamepad_FaceButton_Left)   return TEXT("Face_Left");
	if (Key == EKeys::Gamepad_FaceButton_Top)    return TEXT("Face_Top");
	if (Key == EKeys::Gamepad_LeftShoulder)  return TEXT("Shoulder_L");
	if (Key == EKeys::Gamepad_RightShoulder) return TEXT("Shoulder_R");
	if (Key == EKeys::Gamepad_LeftTrigger)   return TEXT("Trigger_L");
	if (Key == EKeys::Gamepad_RightTrigger)  return TEXT("Trigger_R");
	if (Key == EKeys::Gamepad_DPad_Up)    return TEXT("DPad_Up");
	if (Key == EKeys::Gamepad_DPad_Down)  return TEXT("DPad_Down");
	if (Key == EKeys::Gamepad_DPad_Left)  return TEXT("DPad_Left");
	if (Key == EKeys::Gamepad_DPad_Right) return TEXT("DPad_Right");
	if (Key == EKeys::Gamepad_LeftThumbstick)  return TEXT("Stick_L");
	if (Key == EKeys::Gamepad_RightThumbstick) return TEXT("Stick_R");
	if (Key == EKeys::Gamepad_Special_Left)  return TEXT("Special_Left");
	if (Key == EKeys::Gamepad_Special_Right) return TEXT("Special_Right");

	const FString KeyStr = Key.ToString();
	if (KeyStr.Len() == 1 && FChar::IsAlpha(KeyStr[0]))
	{
		return KeyStr.ToUpper();
	}

	FString Fallback = KeyStr;
	Fallback.ReplaceInline(TEXT(" "), TEXT("_"));
	return Fallback;
}

FString UTAInputIconSubsystem::XboxAssetName(const FString& Logical) const
{
	if (Logical == TEXT("Face_Bottom")) return TEXT("Xbox_Button_A");
	if (Logical == TEXT("Face_Right"))  return TEXT("Xbox_Button_B");
	if (Logical == TEXT("Face_Left"))   return TEXT("Xbox_Button_X");
	if (Logical == TEXT("Face_Top"))    return TEXT("Xbox_Button_Y");
	if (Logical == TEXT("Shoulder_L")) return TEXT("Xbox_LB");
	if (Logical == TEXT("Shoulder_R")) return TEXT("Xbox_RB");
	if (Logical == TEXT("Trigger_L"))  return TEXT("Xbox_LT");
	if (Logical == TEXT("Trigger_R"))  return TEXT("Xbox_RT");
	if (Logical == TEXT("DPad_Up"))    return TEXT("Xbox_D-Pad_Up");
	if (Logical == TEXT("DPad_Down"))  return TEXT("Xbox_D-Pad_Down");
	if (Logical == TEXT("DPad_Left"))  return TEXT("Xbox_D-Pad_Left");
	if (Logical == TEXT("DPad_Right")) return TEXT("Xbox_D-Pad_Right");
	if (Logical == TEXT("Stick_L")) return TEXT("Xbox_Stick_L");
	if (Logical == TEXT("Stick_R")) return TEXT("Xbox_Stick_R");
	if (Logical == TEXT("Special_Left"))  return TEXT("Xbox_Button_View");
	if (Logical == TEXT("Special_Right")) return TEXT("Xbox_Button_Menu");
	return FString::Printf(TEXT("Xbox_%s"), *Logical);
}

FString UTAInputIconSubsystem::PSAssetName(const FString& Logical) const
{
	if (Logical == TEXT("Face_Bottom")) return TEXT("PlayStation_Button_Cross");
	if (Logical == TEXT("Face_Right"))  return TEXT("PlayStation_Button_Circle");
	if (Logical == TEXT("Face_Left"))   return TEXT("PlayStation_Button_Square");
	if (Logical == TEXT("Face_Top"))    return TEXT("PlayStation_Button_Triangle");
	if (Logical == TEXT("Shoulder_L")) return TEXT("PlayStation_Trigger_L1_1");
	if (Logical == TEXT("Shoulder_R")) return TEXT("PlayStation_Trigger_R1_1");
	if (Logical == TEXT("Trigger_L"))  return TEXT("PlayStation_Trigger_L2_1");
	if (Logical == TEXT("Trigger_R"))  return TEXT("PlayStation_Trigger_R2_1");
	if (Logical == TEXT("DPad_Up"))    return TEXT("PlayStation_D-Pad_Up");
	if (Logical == TEXT("DPad_Down"))  return TEXT("PlayStation_D-Pad_Down");
	if (Logical == TEXT("DPad_Left"))  return TEXT("PlayStation_D-Pad_Left");
	if (Logical == TEXT("DPad_Right")) return TEXT("PlayStation_D-Pad_Right");
	if (Logical == TEXT("Stick_L")) return TEXT("PlayStation_Stick_L");
	if (Logical == TEXT("Stick_R")) return TEXT("PlayStation_Stick_R");
	if (Logical == TEXT("Special_Left"))  return TEXT("PlayStation4_Button_Share");
	if (Logical == TEXT("Special_Right")) return TEXT("PlayStation5_Button_Options_1");
	return FString::Printf(TEXT("PlayStation_%s"), *Logical);
}

FString UTAInputIconSubsystem::BuildIconPath(FKey Key) const
{
	const FString Name = KeyToAssetName(Key);
	if (Name.IsEmpty())
	{
		return FString();
	}

	FString Folder;
	FString AssetName;

	switch (CurrentDeviceType)
	{
	case EInputDeviceType::KeyboardMouse:
		Folder = TEXT("Keyboard_Mouse");
		if (Key.IsMouseButton())
		{
			AssetName = FString::Printf(TEXT("Mouse_%s"), *Name);
		}
		else
		{
			AssetName = FString::Printf(TEXT("Keyboard_%s"), *Name);
		}
		break;

	case EInputDeviceType::Xbox:
		Folder = TEXT("Xbox");
		AssetName = XboxAssetName(Name);
		break;

	case EInputDeviceType::PS5:
		Folder = TEXT("PS");
		AssetName = PSAssetName(Name);
		break;

	default:
		Folder = TEXT("Keyboard_Mouse");
		AssetName = FString::Printf(TEXT("Keyboard_%s"), *Name);
		break;
	}

	return FString::Printf(TEXT("%s/%s/%s"), *IconRootPath, *Folder, *AssetName);
}

UTexture2D* UTAInputIconSubsystem::LoadIconFromPath(const FString& Path) const
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}

	return LoadObject<UTexture2D>(nullptr, *Path);
}

UTexture2D* UTAInputIconSubsystem::GetIconForKey(FKey Key) const
{
	const FString Path = BuildIconPath(Key);
	return LoadIconFromPath(Path);
}

UTexture2D* UTAInputIconSubsystem::GetIconForAction(UInputAction* Action) const
{
	if (!Action)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return nullptr;
	}

	const TArray<FKey> MappedKeys = InputSubsystem->QueryKeysMappedToAction(Action);
	if (MappedKeys.Num() == 0)
	{
		return nullptr;
	}

	for (const FKey& Key : MappedKeys)
	{
		if (!Key.IsValid())
		{
			continue;
		}

		if (CurrentDeviceType == EInputDeviceType::KeyboardMouse)
		{
			if (!Key.IsGamepadKey())
			{
				return GetIconForKey(Key);
			}
		}
		else
		{
			if (Key.IsGamepadKey())
			{
				return GetIconForKey(Key);
			}
		}
	}

	return GetIconForKey(MappedKeys[0]);
}

void UTAInputIconSubsystem::RefreshCurrentDeviceForUser(FPlatformUserId UserId)
{
	UInputDeviceSubsystem* DeviceSubsystem = UInputDeviceSubsystem::Get();
	if (!DeviceSubsystem || !UserId.IsValid())
	{
		return;
	}

	const FHardwareDeviceIdentifier Hardware = DeviceSubsystem->GetMostRecentlyUsedHardwareDevice(UserId);
	if (!Hardware.IsValid())
	{
		return;
	}

	switch (Hardware.PrimaryDeviceType)
	{
	case EHardwareDevicePrimaryType::Gamepad:
		SetCurrentDeviceType(EInputDeviceType::Xbox);
		break;
	case EHardwareDevicePrimaryType::KeyboardAndMouse:
		SetCurrentDeviceType(EInputDeviceType::KeyboardMouse);
		break;
	default:
		break;
	}
}
