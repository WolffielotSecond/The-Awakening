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
