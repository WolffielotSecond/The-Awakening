#include "Core/TAGamepadBrandResolver.h"
#include "Misc/App.h"

#if TA_WITH_STEAM_INPUT
#include "SteamSharedModule.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <Xinput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTAGamepadBrand, Log, All);

namespace TAGamepadBrand
{
	// ESteamInputType values (Steamworks SDK 1.57).
	constexpr int32 SteamXBox360 = 2;
	constexpr int32 SteamXBoxOne = 3;
	constexpr int32 SteamPS4 = 5;
	constexpr int32 SteamPS3 = 12;
	constexpr int32 SteamPS5 = 13;
#if TA_WITH_STEAM_INPUT
	static_assert(SteamXBox360 == k_ESteamInputType_XBox360Controller && SteamXBoxOne == k_ESteamInputType_XBoxOneController &&
		SteamPS4 == k_ESteamInputType_PS4Controller && SteamPS3 == k_ESteamInputType_PS3Controller &&
		SteamPS5 == k_ESteamInputType_PS5Controller, "Steam input type values changed");
#endif
	constexpr uint16 SonyVendorId = 0x054C;
	constexpr uint16 ValveVendorId = 0x28DE; // Steam Input virtual pads

	const TCHAR* ToString(ETAGamepadBrand Brand)
	{
		switch (Brand)
		{
		case ETAGamepadBrand::Xbox: return TEXT("Xbox");
		case ETAGamepadBrand::PlayStation: return TEXT("PlayStation");
		default: return TEXT("Unknown");
		}
	}
}

FTAGamepadBrandResolver::~FTAGamepadBrandResolver()
{
	Shutdown();
}

void FTAGamepadBrandResolver::Shutdown()
{
#if TA_WITH_STEAM_INPUT
	if (bSteamInputInitialized && SteamClient.IsValid() && SteamInput())
	{
		SteamInput()->Shutdown();
	}
#endif
	bSteamInputInitialized = false;
	SteamClient.Reset();
	if (XInputModule)
	{
		FPlatformProcess::FreeDllHandle(XInputModule);
		XInputModule = nullptr;
		XInputGetStateFn = nullptr;
	}
	bXInputUnavailable = false;
	ResetXInputSlots();
}

void FTAGamepadBrandResolver::ResetXInputSlots()
{
	for (int32 Slot = 0; Slot < MaxXInputSlots; ++Slot)
	{
		bHasPacketNumber[Slot] = false;
		SlotRecheckTime[Slot] = 0.0;
	}
}

#if PLATFORM_WINDOWS
namespace TAGamepadBrand
{
	constexpr double DisconnectedSlotRecheckInterval = 2.0;

	bool IsHeld(const XINPUT_GAMEPAD& Pad, const FKey& Key)
	{
		struct FButton { const FKey& Key; WORD Mask; };
		const FButton Buttons[] = {
			{EKeys::Gamepad_FaceButton_Bottom, XINPUT_GAMEPAD_A}, {EKeys::Gamepad_FaceButton_Right, XINPUT_GAMEPAD_B},
			{EKeys::Gamepad_FaceButton_Left, XINPUT_GAMEPAD_X}, {EKeys::Gamepad_FaceButton_Top, XINPUT_GAMEPAD_Y},
			{EKeys::Gamepad_DPad_Up, XINPUT_GAMEPAD_DPAD_UP}, {EKeys::Gamepad_DPad_Down, XINPUT_GAMEPAD_DPAD_DOWN},
			{EKeys::Gamepad_DPad_Left, XINPUT_GAMEPAD_DPAD_LEFT}, {EKeys::Gamepad_DPad_Right, XINPUT_GAMEPAD_DPAD_RIGHT},
			{EKeys::Gamepad_Special_Right, XINPUT_GAMEPAD_START}, {EKeys::Gamepad_Special_Left, XINPUT_GAMEPAD_BACK},
			{EKeys::Gamepad_LeftThumbstick, XINPUT_GAMEPAD_LEFT_THUMB}, {EKeys::Gamepad_RightThumbstick, XINPUT_GAMEPAD_RIGHT_THUMB},
			{EKeys::Gamepad_LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER}, {EKeys::Gamepad_RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER},
		};
		for (const FButton& Button : Buttons)
		{
			if (Key == Button.Key) return (Pad.wButtons & Button.Mask) != 0;
		}
		if (Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_LeftTriggerAxis) return Pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		if (Key == EKeys::Gamepad_RightTrigger || Key == EKeys::Gamepad_RightTriggerAxis) return Pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		auto Beyond = [](SHORT X, SHORT Y, int32 DeadZone) { return FMath::Abs(int32(X)) > DeadZone || FMath::Abs(int32(Y)) > DeadZone; };
		if (Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_LeftY || Key == EKeys::Gamepad_Left2D ||
			Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_LeftStick_Down ||
			Key == EKeys::Gamepad_LeftStick_Left || Key == EKeys::Gamepad_LeftStick_Right)
		{
			return Beyond(Pad.sThumbLX, Pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		}
		if (Key == EKeys::Gamepad_RightX || Key == EKeys::Gamepad_RightY || Key == EKeys::Gamepad_Right2D ||
			Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down ||
			Key == EKeys::Gamepad_RightStick_Left || Key == EKeys::Gamepad_RightStick_Right)
		{
			return Beyond(Pad.sThumbRX, Pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
		}
		return false;
	}
}
#endif

int32 FTAGamepadBrandResolver::FindXInputSlotForKey(const FKey& Key)
{
#if PLATFORM_WINDOWS
	using FXInputGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
	if (!XInputGetStateFn)
	{
		if (bXInputUnavailable) return INDEX_NONE;
		XInputModule = FPlatformProcess::GetDllHandle(TEXT("xinput1_4.dll"));
		XInputGetStateFn = XInputModule ? FPlatformProcess::GetDllExport(XInputModule, TEXT("XInputGetState")) : nullptr;
		if (!XInputGetStateFn)
		{
			bXInputUnavailable = true;
			UE_LOG(LogTAGamepadBrand, Log, TEXT("XInput is unavailable; gamepad slots cannot be identified."));
			return INDEX_NONE;
		}
	}
	const FXInputGetState GetState = reinterpret_cast<FXInputGetState>(XInputGetStateFn);
	const double Now = FPlatformTime::Seconds();
	int32 HeldSlot = INDEX_NONE, HeldCount = 0, HeldChangedSlot = INDEX_NONE, HeldChangedCount = 0;
	int32 ChangedSlot = INDEX_NONE, ChangedCount = 0;
	for (int32 Slot = 0; Slot < MaxXInputSlots; ++Slot)
	{
		if (Now < SlotRecheckTime[Slot]) continue;
		XINPUT_STATE State = {};
		if (GetState(Slot, &State) != ERROR_SUCCESS)
		{
			bHasPacketNumber[Slot] = false;
			SlotRecheckTime[Slot] = Now + TAGamepadBrand::DisconnectedSlotRecheckInterval;
			continue;
		}
		const bool bChanged = !bHasPacketNumber[Slot] || State.dwPacketNumber != LastPacketNumbers[Slot];
		LastPacketNumbers[Slot] = State.dwPacketNumber;
		bHasPacketNumber[Slot] = true;
		if (bChanged) { ChangedSlot = Slot; ++ChangedCount; }
		if (TAGamepadBrand::IsHeld(State.Gamepad, Key))
		{
			HeldSlot = Slot; ++HeldCount;
			if (bChanged) { HeldChangedSlot = Slot; ++HeldChangedCount; }
		}
	}
	if (HeldCount == 1) return HeldSlot;
	if (HeldCount > 1) return HeldChangedCount == 1 ? HeldChangedSlot : INDEX_NONE;
	return ChangedCount == 1 ? ChangedSlot : INDEX_NONE;
#else
	return INDEX_NONE;
#endif
}

ETAGamepadBrand FTAGamepadBrandResolver::FromSteamInputType(int32 SteamInputType)
{
	using namespace TAGamepadBrand;
	switch (SteamInputType)
	{
	case SteamPS3:
	case SteamPS4:
	case SteamPS5:
		return ETAGamepadBrand::PlayStation;
	case 0: // k_ESteamInputType_Unknown
		return ETAGamepadBrand::Unknown;
	default:
		return ETAGamepadBrand::Xbox;
	}
}

ETAGamepadBrand FTAGamepadBrandResolver::FromVendorIds(const TArray<uint16>& VendorIds)
{
	ETAGamepadBrand Result = ETAGamepadBrand::Unknown;
	for (const uint16 VendorId : VendorIds)
	{
		if (VendorId == TAGamepadBrand::ValveVendorId) continue;
		const ETAGamepadBrand Brand = VendorId == TAGamepadBrand::SonyVendorId ? ETAGamepadBrand::PlayStation : ETAGamepadBrand::Xbox;
		if (Result != ETAGamepadBrand::Unknown && Result != Brand) return ETAGamepadBrand::Unknown;
		Result = Brand;
	}
	return Result;
}

ETAGamepadBrand FTAGamepadBrandResolver::Resolve(int32 XInputSlot)
{
	bool bSteamAvailable = false;
	ETAGamepadBrand Brand = ResolveWithSteam(XInputSlot, bSteamAvailable);
	const TCHAR* Source = TEXT("Steam");
	if (!bSteamAvailable)
	{
		Brand = FromVendorIds(GetConnectedGamepadVendorIds());
		Source = TEXT("HID");
	}
	UE_LOG(LogTAGamepadBrand, Log, TEXT("Gamepad slot %d resolved as %s (%s)"), XInputSlot, TAGamepadBrand::ToString(Brand), Source);
	return Brand;
}

ETAGamepadBrand FTAGamepadBrandResolver::ResolveWithSteam(int32 XInputSlot, bool& bSteamAvailable)
{
	bSteamAvailable = false;
#if TA_WITH_STEAM_INPUT
	// The editor shares one process for every PIE session; Steam is tested in standalone and packaged builds.
	if (bSteamUnavailable || GIsEditor) return ETAGamepadBrand::Unknown;
	if (!bSteamInputInitialized)
	{
		SteamClient = FSteamSharedModule::Get().ObtainSteamClientInstanceHandle();
		if (!SteamClient.IsValid() || !SteamInput() || !SteamInput()->Init(false))
		{
			bSteamUnavailable = true;
			SteamClient.Reset();
			UE_LOG(LogTAGamepadBrand, Log, TEXT("Steam Input is unavailable; using HID vendor ids."));
			return ETAGamepadBrand::Unknown;
		}
		bSteamInputInitialized = true;
	}
	ISteamInput* Input = SteamInput();
	Input->RunFrame();
	InputHandle_t Handles[STEAM_INPUT_MAX_COUNT];
	const int32 Count = Input->GetConnectedControllers(Handles);
	if (Count <= 0) return ETAGamepadBrand::Unknown; // Steam Input does not own any pad: physical XInput or HID.
	bSteamAvailable = true;
	if (XInputSlot != INDEX_NONE)
	{
		// Zero means the slot is a physical XInput pad that Steam does not emulate.
		const InputHandle_t Handle = Input->GetControllerForGamepadIndex(XInputSlot);
		return Handle ? FromSteamInputType(Input->GetInputTypeForHandle(Handle)) : ETAGamepadBrand::Xbox;
	}
	ETAGamepadBrand Result = ETAGamepadBrand::Unknown;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const ETAGamepadBrand Brand = FromSteamInputType(Input->GetInputTypeForHandle(Handles[Index]));
		if (Result != ETAGamepadBrand::Unknown && Brand != Result) return ETAGamepadBrand::Unknown;
		Result = Brand;
	}
	return Result;
#else
	return ETAGamepadBrand::Unknown;
#endif
}

TArray<uint16> FTAGamepadBrandResolver::GetConnectedGamepadVendorIds()
{
	TArray<uint16> VendorIds;
#if PLATFORM_WINDOWS
	UINT DeviceCount = 0;
	if (GetRawInputDeviceList(nullptr, &DeviceCount, sizeof(RAWINPUTDEVICELIST)) != 0 || DeviceCount == 0) return VendorIds;
	TArray<RAWINPUTDEVICELIST> Devices;
	Devices.SetNumZeroed(DeviceCount);
	const UINT Listed = GetRawInputDeviceList(Devices.GetData(), &DeviceCount, sizeof(RAWINPUTDEVICELIST));
	if (Listed == static_cast<UINT>(-1)) return VendorIds;
	for (UINT Index = 0; Index < Listed; ++Index)
	{
		if (Devices[Index].dwType != RIM_TYPEHID) continue;
		RID_DEVICE_INFO Info = {};
		Info.cbSize = sizeof(Info);
		UINT Size = sizeof(Info);
		if (GetRawInputDeviceInfoW(Devices[Index].hDevice, RIDI_DEVICEINFO, &Info, &Size) == static_cast<UINT>(-1)) continue;
		// Generic desktop page: joystick (4) or gamepad (5).
		if (Info.hid.usUsagePage == 1 && (Info.hid.usUsage == 4 || Info.hid.usUsage == 5))
		{
			VendorIds.Add(static_cast<uint16>(Info.hid.dwVendorId));
		}
	}
#endif
	return VendorIds;
}
