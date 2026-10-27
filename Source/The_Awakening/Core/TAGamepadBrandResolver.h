#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

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
	/**
	 * UE 5.8 no longer reports which XInput slot sent an input, so the slot is read back from XInput:
	 * the pad whose current state holds Key, otherwise the only pad whose packet number changed.
	 * Returns INDEX_NONE when ambiguous, off Windows, or for non-XInput pads (DualSense without Steam Input).
	 */
	int32 FindXInputSlotForKey(const FKey& Key);
	/** Disconnected slots are skipped for a while; call when devices connect or disconnect to query them again. */
	void ResetXInputSlots();
	/** Releases the Steam Input API, the Steam client handle and the XInput library acquired by this resolver. */
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

	static constexpr int32 MaxXInputSlots = 4;
	void* XInputModule = nullptr;
	void* XInputGetStateFn = nullptr;
	bool bXInputUnavailable = false;
	uint32 LastPacketNumbers[MaxXInputSlots] = {};
	bool bHasPacketNumber[MaxXInputSlots] = {};
	/** Querying a disconnected slot is slow; such slots are rechecked only after this time. */
	double SlotRecheckTime[MaxXInputSlots] = {};
};
