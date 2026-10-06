#pragma once
#include "CoreMinimal.h"
#include "Settings/TASettingsTypes.h"
class ULocalPlayer;
/** Runtime consumers only. Does not own definitions, values, persistence or widgets. */
struct THE_AWAKENING_API FTASettingsApplyService
{
 static bool Apply(ULocalPlayer* Player, FName Handler, const FTASettingValue& Value);
 static bool ReadDisplayValue(FName Handler, FTASettingValue& Value);
};
