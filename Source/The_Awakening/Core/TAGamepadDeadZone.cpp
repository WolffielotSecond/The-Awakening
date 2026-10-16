#include "Core/TAGamepadDeadZone.h"

float FTAGamepadDeadZone::Remap(float Magnitude, const FTADeadZoneRange& Range)
{
	if (Magnitude < Range.Min) return 0.f;
	const float Span = 1.f - Range.Max - Range.Min;
	if (Span <= UE_KINDA_SMALL_NUMBER) return 1.f;
	return FMath::Clamp((Magnitude - Range.Min) / Span, 0.f, 1.f);
}

FVector2D FTAGamepadDeadZone::RemapStick(const FVector2D& Raw, const FTADeadZoneRange& Range)
{
	const float Size = Raw.Size();
	if (Size <= UE_KINDA_SMALL_NUMBER) return FVector2D::ZeroVector;
	return Raw / Size * Remap(FMath::Min(Size, 1.f), Range);
}

bool FTAGamepadDeadZone::FindAnalogInput(const FKey& Key, ETADeadZoneInput& OutInput, int32& OutAxis)
{
	OutAxis = 0;
	if (Key == EKeys::Gamepad_LeftX) { OutInput = ETADeadZoneInput::LeftStick; return true; }
	if (Key == EKeys::Gamepad_LeftY) { OutInput = ETADeadZoneInput::LeftStick; OutAxis = 1; return true; }
	if (Key == EKeys::Gamepad_RightX) { OutInput = ETADeadZoneInput::RightStick; return true; }
	if (Key == EKeys::Gamepad_RightY) { OutInput = ETADeadZoneInput::RightStick; OutAxis = 1; return true; }
	if (Key == EKeys::Gamepad_LeftTriggerAxis) { OutInput = ETADeadZoneInput::LeftTrigger; return true; }
	if (Key == EKeys::Gamepad_RightTriggerAxis) { OutInput = ETADeadZoneInput::RightTrigger; return true; }
	return false;
}

bool FTAGamepadDeadZone::FindVirtualKeyInput(const FKey& Key, ETADeadZoneInput& OutInput)
{
	if (Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_LeftStick_Down ||
		Key == EKeys::Gamepad_LeftStick_Left || Key == EKeys::Gamepad_LeftStick_Right)
	{
		OutInput = ETADeadZoneInput::LeftStick; return true;
	}
	if (Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down ||
		Key == EKeys::Gamepad_RightStick_Left || Key == EKeys::Gamepad_RightStick_Right)
	{
		OutInput = ETADeadZoneInput::RightStick; return true;
	}
	if (Key == EKeys::Gamepad_LeftTrigger) { OutInput = ETADeadZoneInput::LeftTrigger; return true; }
	if (Key == EKeys::Gamepad_RightTrigger) { OutInput = ETADeadZoneInput::RightTrigger; return true; }
	return false;
}

FKey FTAGamepadDeadZone::GetOtherStickAxis(const FKey& Key)
{
	if (Key == EKeys::Gamepad_LeftX) return EKeys::Gamepad_LeftY;
	if (Key == EKeys::Gamepad_LeftY) return EKeys::Gamepad_LeftX;
	if (Key == EKeys::Gamepad_RightX) return EKeys::Gamepad_RightY;
	if (Key == EKeys::Gamepad_RightY) return EKeys::Gamepad_RightX;
	return EKeys::Invalid;
}

FName FTAGamepadDeadZone::GetSettingId(ETADeadZoneInput Input, bool bMax)
{
	static const FName Ids[InputCount][2] = {
		{TEXT("Controller.LeftTriggerDeadZoneMin"), TEXT("Controller.LeftTriggerDeadZoneMax")},
		{TEXT("Controller.LeftStickDeadZoneMin"), TEXT("Controller.LeftStickDeadZoneMax")},
		{TEXT("Controller.RightStickDeadZoneMin"), TEXT("Controller.RightStickDeadZoneMax")},
		{TEXT("Controller.RightTriggerDeadZoneMin"), TEXT("Controller.RightTriggerDeadZoneMax")}};
	return Input < ETADeadZoneInput::Count ? Ids[Index(Input)][bMax ? 1 : 0] : NAME_None;
}

float FTAGamepadDeadZone::Magnitude(ETADeadZoneInput Input) const
{
	const FVector2D& Value = Raw[Index(Input)];
	return IsStick(Input) ? FMath::Min(Value.Size(), 1.f) : FMath::Clamp(FMath::Abs(Value.X), 0.f, 1.f);
}

bool FTAGamepadDeadZone::IsActive(ETADeadZoneInput Input) const
{
	const float Size = Magnitude(Input);
	return Size > 0.f && !IsIgnoredMagnitude(Size, GetRange(Input));
}

float FTAGamepadDeadZone::Filter(const FKey& Key, float RawValue, bool& bOutIgnored)
{
	ETADeadZoneInput Input; int32 Axis = 0;
	bOutIgnored = false;
	if (!FindAnalogInput(Key, Input, Axis)) return RawValue;
	FVector2D& Value = Raw[Index(Input)];
	if (Axis == 0) Value.X = RawValue; else Value.Y = RawValue;
	bOutIgnored = !IsActive(Input);
	return GetFilteredAxis(Key);
}

float FTAGamepadDeadZone::GetFilteredAxis(const FKey& Key) const
{
	ETADeadZoneInput Input; int32 Axis = 0;
	if (!FindAnalogInput(Key, Input, Axis)) return 0.f;
	const FVector2D& Value = Raw[Index(Input)];
	if (!IsStick(Input))
	{
		return FMath::Sign(Value.X) * Remap(FMath::Clamp(FMath::Abs(Value.X), 0.f, 1.f), GetRange(Input));
	}
	const FVector2D Filtered = RemapStick(Value, GetRange(Input));
	return Axis == 0 ? Filtered.X : Filtered.Y;
}

void FTAGamepadDeadZone::Reset()
{
	for (FVector2D& Value : Raw) Value = FVector2D::ZeroVector;
}
