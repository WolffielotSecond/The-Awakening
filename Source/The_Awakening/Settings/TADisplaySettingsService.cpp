#include "Settings/TADisplaySettingsService.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/SWindow.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformTime.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
 FName ResolutionKey(FIntPoint R) { return FName(*FString::Printf(TEXT("%dx%d"),R.X,R.Y)); }
 bool Valid(FIntPoint R) { return R.X>0 && R.Y>0; }
 TSharedPtr<SWindow> GameWindow()
 {
  UGameViewportClient* V=GEngine?GEngine->GameViewport.Get():nullptr;
  // Never move the editor's window or its embedded PIE viewport.
  return V && V->GetWorld() && V->GetWorld()->WorldType==EWorldType::Game?V->GetWindow():nullptr;
 }
 const FTADisplayMonitor* FindMonitor(const TArray<FTADisplayMonitor>& Monitors,FName Id)
 { return Monitors.FindByPredicate([Id](const auto& M){return M.Id==Id;}); }
 void SortResolutions(TArray<FIntPoint>& Values)
 { Values.Sort([](FIntPoint A,FIntPoint B){return A.X==B.X?A.Y<B.Y:A.X<B.X;}); }
#if PLATFORM_WINDOWS
 BOOL CALLBACK CollectMonitor(HMONITOR Handle,HDC,LPRECT,LPARAM Data)
 {
  MONITORINFOEXW Info={}; Info.cbSize=sizeof(Info);
  if (!GetMonitorInfoW(Handle,&Info)) return 1;
  FTADisplayMonitor M;
  DISPLAY_DEVICEW Device={}; Device.cb=sizeof(Device);
  const bool Found=EnumDisplayDevicesW(Info.szDevice,0,&Device,0)!=0;
  M.Id=FName(Found && Device.DeviceID[0]?Device.DeviceID:Info.szDevice);
  M.Name=Found && Device.DeviceString[0]?Device.DeviceString:Info.szDevice;
  M.bPrimary=(Info.dwFlags&MONITORINFOF_PRIMARY)!=0;
  M.DisplayRect={Info.rcMonitor.left,Info.rcMonitor.top,Info.rcMonitor.right,Info.rcMonitor.bottom};
  M.WorkArea={Info.rcWork.left,Info.rcWork.top,Info.rcWork.right,Info.rcWork.bottom};
  M.Desktop=FIntPoint(Info.rcMonitor.right-Info.rcMonitor.left,Info.rcMonitor.bottom-Info.rcMonitor.top);
  DEVMODEW DesktopMode={}; DesktopMode.dmSize=sizeof(DesktopMode);
  if (EnumDisplaySettingsW(Info.szDevice,ENUM_REGISTRY_SETTINGS,&DesktopMode) && DesktopMode.dmPelsWidth>0 && DesktopMode.dmPelsHeight>0)
   M.Desktop=FIntPoint(DesktopMode.dmPelsWidth,DesktopMode.dmPelsHeight);
  DEVMODEW Mode={}; Mode.dmSize=sizeof(Mode);
  for (DWORD I=0;EnumDisplaySettingsW(Info.szDevice,I,&Mode);++I)
   if (Mode.dmPelsWidth>0 && Mode.dmPelsHeight>0) M.FullscreenResolutions.AddUnique(FIntPoint(Mode.dmPelsWidth,Mode.dmPelsHeight));
  M.FullscreenResolutions.AddUnique(M.Desktop); SortResolutions(M.FullscreenResolutions);
  reinterpret_cast<TArray<FTADisplayMonitor>*>(Data)->Add(M);
  return 1;
 }
#endif
}
TArray<FTADisplayMonitor> FTADisplaySettingsService::GetMonitors()
{
 static TArray<FTADisplayMonitor> Cached;
 static double LastRead=-1;
 const double Now=FPlatformTime::Seconds();
 if (!Cached.IsEmpty() && Now-LastRead<0.5) return Cached;
 TArray<FTADisplayMonitor> Result;
#if PLATFORM_WINDOWS
 EnumDisplayMonitors(nullptr,nullptr,CollectMonitor,reinterpret_cast<LPARAM>(&Result));
#else
 FDisplayMetrics Metrics; FDisplayMetrics::RebuildDisplayMetrics(Metrics);
 for (const auto& Info:Metrics.MonitorInfo)
 {
  FTADisplayMonitor M; M.Id=FName(*Info.ID); M.Name=Info.Name; M.DisplayRect=Info.DisplayRect; M.WorkArea=Info.WorkArea; M.bPrimary=Info.bIsPrimary;
  M.Desktop=FIntPoint(Info.DisplayRect.Right-Info.DisplayRect.Left,Info.DisplayRect.Bottom-Info.DisplayRect.Top);
  UKismetSystemLibrary::GetSupportedFullscreenResolutions(M.FullscreenResolutions);
  M.FullscreenResolutions.AddUnique(M.Desktop); Result.Add(M);
 }
#endif
 if (Result.IsEmpty())
 {
  FTADisplayMonitor M; M.Id=TEXT("Primary"); M.Name=TEXT("Primary"); M.bPrimary=true;
  M.DisplayRect={0,0,1280,720}; M.WorkArea=M.DisplayRect; M.FullscreenResolutions.Add(M.Desktop); Result.Add(M);
 }
 Result.Sort([](const auto& A,const auto& B){return A.bPrimary!=B.bPrimary?A.bPrimary:A.Id.LexicalLess(B.Id);});
 Cached=Result; LastRead=Now; return Result;
}
FTADisplayState FTADisplaySettingsService::GetCurrentState(const TArray<FTADisplayMonitor>& Monitors)
{
 FTADisplayState State;
 if (auto* U=UGameUserSettings::GetGameUserSettings())
 { State.Resolution=U->GetScreenResolution(); State.WindowMode=U->GetFullscreenMode(); State.WindowPosition=U->GetWindowPosition(); }
 const auto Window=GameWindow();
 if (Window) { const auto Position=Window->GetPositionInScreen(); State.WindowPosition=FIntPoint(FMath::RoundToInt(Position.X),FMath::RoundToInt(Position.Y)); }
 const FTADisplayMonitor* Selected=Monitors.IsEmpty()?nullptr:&Monitors[0];
 // Largest intersection also works during a deferred resize after a monitor move.
 const FVector2D Size=Window?Window->GetSizeInScreen():FVector2D(State.Resolution);
 double Largest=0;
 for (const auto& M:Monitors)
 {
  const double Width=FMath::Max(0.,FMath::Min(double(State.WindowPosition.X)+Size.X,double(M.DisplayRect.Right))-FMath::Max(double(State.WindowPosition.X),double(M.DisplayRect.Left)));
  const double Height=FMath::Max(0.,FMath::Min(double(State.WindowPosition.Y)+Size.Y,double(M.DisplayRect.Bottom))-FMath::Max(double(State.WindowPosition.Y),double(M.DisplayRect.Top)));
  if (Width*Height>Largest) { Largest=Width*Height; Selected=&M; }
 }
 if (Selected)
 {
  State.MonitorId=Selected->Id;
  if (!Valid(State.Resolution) || State.WindowMode==EWindowMode::WindowedFullscreen) State.Resolution=Selected->Desktop;
 }
 return State;
}
FName FTADisplaySettingsService::AspectRatio(FIntPoint R)
{
 if (!Valid(R)) return NAME_None;
 const double Ratio=double(R.X)/R.Y;
 for (const FIntPoint Known:{FIntPoint(4,3),FIntPoint(5,4),FIntPoint(16,9),FIntPoint(16,10),FIntPoint(21,9),FIntPoint(32,9),FIntPoint(9,16)})
  if (FMath::Abs(Ratio-double(Known.X)/Known.Y)<(Known.X==21?0.08:0.01)) return FName(*FString::Printf(TEXT("%d:%d"),Known.X,Known.Y));
 int32 A=R.X,B=R.Y; while (B) { const int32 T=A%B; A=B; B=T; }
 return FName(*FString::Printf(TEXT("%d:%d"),R.X/A,R.Y/A));
}
TArray<FIntPoint> FTADisplaySettingsService::GetResolutions(const FTADisplayMonitor& M,int32 Mode)
{
 if (Mode==EWindowMode::WindowedFullscreen) return {M.Desktop};
 TArray<FIntPoint> Values=M.FullscreenResolutions;
 Values.RemoveAll([](FIntPoint R){return !Valid(R);}); Values.AddUnique(M.Desktop);
 if (Mode==EWindowMode::Windowed)
 {
  for (FIntPoint R:{FIntPoint(800,600),FIntPoint(1024,768),FIntPoint(1280,720),FIntPoint(1280,800),FIntPoint(1280,960),FIntPoint(1366,768),FIntPoint(1440,900),FIntPoint(1600,900),FIntPoint(1680,1050),FIntPoint(1920,1080),FIntPoint(1920,1200),FIntPoint(2560,1080),FIntPoint(2560,1440),FIntPoint(3440,1440),FIntPoint(3840,2160)})
   if (R.X<=M.Desktop.X && R.Y<=M.Desktop.Y) Values.AddUnique(R);
  Values.RemoveAll([&](FIntPoint R){return R.X>M.Desktop.X || R.Y>M.Desktop.Y;});
 }
 SortResolutions(Values); return Values;
}
FIntPoint FTADisplaySettingsService::ChooseResolution(const TArray<FIntPoint>& Values,FIntPoint Previous,FName Aspect)
{
 FIntPoint Best=FIntPoint::ZeroValue; double Distance=TNumericLimits<double>::Max();
 for (FIntPoint R:Values)
 {
  if (!Valid(R) || (!Aspect.IsNone() && AspectRatio(R)!=Aspect)) continue;
  const double D=FMath::Abs(double(R.X)*R.Y-double(Previous.X)*Previous.Y);
  if (D<Distance) { Distance=D; Best=R; }
 }
 return Best;
}
FName FTADisplaySettingsService::ReadChoice(FName Handler,const TArray<FTADisplayMonitor>&,const FTADisplayState& State)
{
 if (Handler==TEXT("Display.Monitor")) return State.MonitorId;
 if (Handler==TEXT("Display.AspectRatio")) return AspectRatio(State.Resolution);
 if (Handler==TEXT("Display.Resolution")) return ResolutionKey(State.Resolution);
 if (Handler==TEXT("Display.WindowMode")) return State.WindowMode==EWindowMode::Fullscreen?TEXT("Fullscreen"):State.WindowMode==EWindowMode::WindowedFullscreen?TEXT("WindowedFullscreen"):TEXT("Windowed");
 return NAME_None;
}
bool FTADisplaySettingsService::IsVideoSetting(FName Handler)
{ return Handler==TEXT("Display.Monitor") || Handler==TEXT("Display.WindowMode") || Handler==TEXT("Display.AspectRatio") || Handler==TEXT("Display.Resolution"); }
void FTADisplaySettingsService::RefreshDefinition(FTASettingDefinition& D,const TArray<FTADisplayMonitor>& Monitors,const FTADisplayState& State)
{
 const auto* M=FindMonitor(Monitors,State.MonitorId); if (!M) return;
 auto Add=[&](FName V){FTASettingChoice C; C.Value=V; D.Choices.Add(C);};
 if (D.ChoiceProviderId==TEXT("DisplayMonitors"))
 { D.Choices.Reset(); for (const auto& Monitor:Monitors) Add(Monitor.Id); D.DefaultValue=0; }
 else if (D.ChoiceProviderId==TEXT("ScreenResolutions") || D.ChoiceProviderId==TEXT("DisplayAspectRatios"))
 {
  auto Resolutions=GetResolutions(*M,State.WindowMode);
  // Keep the current value readable even if an old window size is not a standard mode.
  if (State.WindowMode!=EWindowMode::WindowedFullscreen && Valid(State.Resolution)) Resolutions.AddUnique(State.Resolution);
  SortResolutions(Resolutions); D.Choices.Reset();
  const FName Aspect=AspectRatio(State.Resolution);
  for (FIntPoint R:Resolutions)
  {
   const FName Key=D.ChoiceProviderId==TEXT("DisplayAspectRatios")?AspectRatio(R):ResolutionKey(R);
   if (D.ChoiceProviderId==TEXT("ScreenResolutions") && AspectRatio(R)!=Aspect) continue;
   if (D.FindChoiceIndex(Key)==INDEX_NONE) Add(Key);
  }
  // Defaults are resolved against the current device/list rather than saved indices.
  D.DefaultValue=D.FindChoiceIndex(D.ChoiceProviderId==TEXT("DisplayAspectRatios")?AspectRatio(M->Desktop):ResolutionKey(M->Desktop));
  if (D.DefaultValue==INDEX_NONE) D.DefaultValue=D.Choices.Num()-1;
 }
}
bool FTADisplaySettingsService::ApplyState(const FTADisplayState& State)
{
 const auto Window=GameWindow(); auto* U=UGameUserSettings::GetGameUserSettings();
 if (!Window || !U || !Valid(State.Resolution)) return false;
 auto SafeState=State;
 const auto Monitors=GetMonitors();
 if (!FindMonitor(Monitors,State.MonitorId) && !Monitors.IsEmpty())
 {
  // A disconnected display must not leave the restored window off-screen.
  SafeState.MonitorId=Monitors[0].Id;
  SafeState.Resolution=ChooseResolution(GetResolutions(Monitors[0],State.WindowMode),State.Resolution);
  SafeState.WindowPosition=FIntPoint(Monitors[0].DisplayRect.Left,Monitors[0].DisplayRect.Top);
 }
 // Leave exclusive fullscreen before moving to a different adapter/display.
 if (U->GetFullscreenMode()==EWindowMode::Fullscreen)
 { U->SetFullscreenMode(EWindowMode::Windowed); U->ApplyResolutionSettings(false); }
 Window->MoveWindowTo(FVector2D(SafeState.WindowPosition));
 U->SetWindowPosition(SafeState.WindowPosition.X,SafeState.WindowPosition.Y);
 U->SetScreenResolution(SafeState.Resolution); U->SetFullscreenMode(static_cast<EWindowMode::Type>(SafeState.WindowMode));
 U->ApplyResolutionSettings(false); return true;
}
bool FTADisplaySettingsService::ApplyChange(FName Handler,FName Choice)
{
 const auto Monitors=GetMonitors(); FTADisplayState Target;
 return BuildChange(Handler,Choice,Monitors,GetCurrentState(Monitors),Target) && ApplyState(Target);
}
bool FTADisplaySettingsService::BuildChange(FName Handler,FName Choice,const TArray<FTADisplayMonitor>& Monitors,const FTADisplayState& Current,FTADisplayState& State)
{
 if (!IsVideoSetting(Handler)) return false;
 State=Current;
 if (Handler==TEXT("Display.Monitor")) State.MonitorId=Choice;
 if (Handler==TEXT("Display.WindowMode"))
 {
  if (Choice!=TEXT("Fullscreen") && Choice!=TEXT("WindowedFullscreen") && Choice!=TEXT("Windowed")) return false;
  State.WindowMode=Choice==TEXT("Fullscreen")?EWindowMode::Fullscreen:Choice==TEXT("WindowedFullscreen")?EWindowMode::WindowedFullscreen:EWindowMode::Windowed;
 }
 const auto* M=FindMonitor(Monitors,State.MonitorId); if (!M) return false;
 const auto Resolutions=GetResolutions(*M,State.WindowMode);
 if (Handler==TEXT("Display.Resolution"))
 {
  FString W,H; int32 X=0,Y=0;
  if (!Choice.ToString().Split(TEXT("x"),&W,&H) || !LexTryParseString(X,*W) || !LexTryParseString(Y,*H)) return false;
  const FIntPoint Requested(X,Y); if (!Resolutions.Contains(Requested) && Requested!=State.Resolution) return false; State.Resolution=Requested;
 }
 else if (State.WindowMode==EWindowMode::WindowedFullscreen) State.Resolution=M->Desktop;
 else
 {
  const FName Aspect=Handler==TEXT("Display.AspectRatio")?Choice:AspectRatio(State.Resolution);
  auto Target=ChooseResolution(Resolutions,State.Resolution,Aspect);
  if (!Valid(Target) && Handler==TEXT("Display.AspectRatio")) return false;
  if (!Valid(Target)) Target=ChooseResolution(Resolutions,M->Desktop);
  State.Resolution=Target;
 }
 const auto& Rect=State.WindowMode==EWindowMode::Windowed?M->WorkArea:M->DisplayRect;
 State.WindowPosition=FIntPoint(Rect.Left+FMath::Max(0,(Rect.Right-Rect.Left-State.Resolution.X)/2),Rect.Top+FMath::Max(0,(Rect.Bottom-Rect.Top-State.Resolution.Y)/2));
 return Valid(State.Resolution);
}
