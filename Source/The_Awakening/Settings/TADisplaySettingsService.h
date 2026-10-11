#pragma once
#include "CoreMinimal.h"
#include "GenericPlatform/GenericApplication.h"
#include "Settings/TASettingsTypes.h"

struct FTADisplayMonitor
{
 FName Id;
 FString Name;
 FPlatformRect DisplayRect;
 FPlatformRect WorkArea;
 FIntPoint Desktop=FIntPoint(1280,720);
 bool bPrimary=false;
 TArray<FIntPoint> FullscreenResolutions;
};
struct FTADisplayState
{
 FName MonitorId;
 FIntPoint Resolution=FIntPoint(1280,720);
 int32 WindowMode=0;
 FIntPoint WindowPosition=FIntPoint::ZeroValue;
};
/** Hardware discovery and window application; no settings values or widgets owned here. */
struct THE_AWAKENING_API FTADisplaySettingsService
{
 static TArray<FTADisplayMonitor> GetMonitors();
 static FTADisplayState GetCurrentState(const TArray<FTADisplayMonitor>& Monitors);
 static FName AspectRatio(FIntPoint Resolution);
 static TArray<FIntPoint> GetResolutions(const FTADisplayMonitor& Monitor,int32 WindowMode);
 static FIntPoint ChooseResolution(const TArray<FIntPoint>& Candidates,FIntPoint Previous,FName Aspect=NAME_None);
 static void RefreshDefinition(FTASettingDefinition& Definition,const TArray<FTADisplayMonitor>& Monitors,const FTADisplayState& State);
 static bool ApplyChange(FName Handler,FName Choice);
 static bool BuildChange(FName Handler,FName Choice,const TArray<FTADisplayMonitor>& Monitors,const FTADisplayState& Current,FTADisplayState& Target);
 static bool ApplyState(const FTADisplayState& State);
 static bool IsVideoSetting(FName Handler);
 static FName ReadChoice(FName Handler,const TArray<FTADisplayMonitor>& Monitors,const FTADisplayState& State);
};
