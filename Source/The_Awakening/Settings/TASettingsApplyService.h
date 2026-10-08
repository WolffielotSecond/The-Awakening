#pragma once
#include "CoreMinimal.h"
#include "Settings/TASettingsTypes.h"
class ULocalPlayer;
/** Runtime consumers only. Does not own definitions, values, persistence or widgets. */
struct THE_AWAKENING_API FTASettingsApplyService
{
 static bool Apply(ULocalPlayer* Player, const FTASettingDefinition& Definition, int32 Value);
 static bool IsDisplaySetting(FName Handler);
 static bool ReadDisplayValue(const FTASettingDefinition& Definition, int32& Value);
};
