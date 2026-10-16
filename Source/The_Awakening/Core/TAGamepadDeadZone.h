#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/** Inputs in the order the settings and the preview present them. */
enum class ETADeadZoneInput : uint8
{
	LeftTrigger,
	LeftStick,
	RightStick,
	RightTrigger,
	Count
};

/** Min is the inner dead zone; Max is the outer dead zone measured inward from full travel. */
struct FTADeadZoneRange
{
	float Min = 0.2f;
	float Max = 0.05f;
};

/**
 * Player dead zones for sticks (radial) and triggers.
 * Below Min the input is ignored; Min maps to 0, (1 - Max) and above map to 1.
 * Raw values are kept so the settings preview can show unfiltered input.
 */
class THE_AWAKENING_API FTAGamepadDeadZone
{
public:
	static constexpr int32 InputCount = static_cast<int32>(ETADeadZoneInput::Count);

	/** Remaps a magnitude in [0, 1]; returns 0 below Min. */
	static float Remap(float Magnitude, const FTADeadZoneRange& Range);
	static bool IsIgnoredMagnitude(float Magnitude, const FTADeadZoneRange& Range) { return Magnitude < Range.Min; }
	/** Radial stick remap keeping direction. */
	static FVector2D RemapStick(const FVector2D& Raw, const FTADeadZoneRange& Range);

	/** Analog keys handled here: stick X/Y axes and trigger axes. */
	static bool FindAnalogInput(const FKey& Key, ETADeadZoneInput& OutInput, int32& OutAxis);
	/** Engine-generated digital keys: stick directions and trigger presses. */
	static bool FindVirtualKeyInput(const FKey& Key, ETADeadZoneInput& OutInput);
	/** Gamepad_LeftX <-> Gamepad_LeftY, Gamepad_RightX <-> Gamepad_RightY; invalid otherwise. */
	static FKey GetOtherStickAxis(const FKey& Key);
	/** Settings ids in preview order: [input][0]=min, [input][1]=max. */
	static FName GetSettingId(ETADeadZoneInput Input, bool bMax);

	void SetRange(ETADeadZoneInput Input, const FTADeadZoneRange& Range) { Ranges[Index(Input)] = Range; }
	const FTADeadZoneRange& GetRange(ETADeadZoneInput Input) const { return Ranges[Index(Input)]; }

	/** Records the raw axis value. Returns the filtered value of that axis; bOutIgnored is true below Min. */
	float Filter(const FKey& Key, float RawValue, bool& bOutIgnored);
	/** Filtered value of an analog key from the recorded raw state (0 when not a dead-zone key). */
	float GetFilteredAxis(const FKey& Key) const;
	/** Raw stick position, or (trigger, 0). */
	FVector2D GetRaw(ETADeadZoneInput Input) const { return Raw[Index(Input)]; }
	/** True when the current raw input is outside the inner dead zone. */
	bool IsActive(ETADeadZoneInput Input) const;
	void Reset();

private:
	static int32 Index(ETADeadZoneInput Input) { return static_cast<int32>(Input); }
	static bool IsStick(ETADeadZoneInput Input) { return Input == ETADeadZoneInput::LeftStick || Input == ETADeadZoneInput::RightStick; }
	float Magnitude(ETADeadZoneInput Input) const;

	FVector2D Raw[InputCount] = {};
	FTADeadZoneRange Ranges[InputCount] = {
		{0.05f, 0.05f}, {0.2f, 0.05f}, {0.2f, 0.05f}, {0.05f, 0.05f}};
};
