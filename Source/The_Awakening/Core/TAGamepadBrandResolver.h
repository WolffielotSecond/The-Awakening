#pragma once

#include "CoreMinimal.h"

class FSteamClientInstanceHandler;

/** Physical gamepad family used only for prompt icons. */
enum class ETAGamepadBrand : uint8
{
	Unknown,
	Xbox,
	PlayStation
};

/**
 * Identifies the physical gamepad behind an XInput slot.
 * Steam Input reports the real controller type even when it emulates an Xbox pad;
 * without Steam, connected HID vendor ids are used when they are unambiguous.
 */
class THE_AWAKENING_API FTAGamepadBrandResolver
{
public:
	~FTAGamepadBrandResolver();

	/** XInputSlot is the XInput user index (0-3); INDEX_NONE queries every connected pad. */
	ETAGamepadBrand Resolve(int32 XInputSlot);
	/** Releases the Steam Input API and the Steam client handle acquired by Resolve. */
	void Shutdown();

	/** Maps ESteamInputType values. Switch and other families use Xbox prompts. */
	static ETAGamepadBrand FromSteamInputType(int32 SteamInputType);
	/** Unknown unless every non-Microsoft, non-Valve gamepad belongs to one brand. */
	static ETAGamepadBrand FromVendorIds(const TArray<uint16>& VendorIds);

private:
	ETAGamepadBrand ResolveWithSteam(int32 XInputSlot, bool& bSteamAvailable);
	static TArray<uint16> GetConnectedGamepadVendorIds();

	TSharedPtr<FSteamClientInstanceHandler> SteamClient;
	bool bSteamInputInitialized = false;
	bool bSteamUnavailable = false;
};
