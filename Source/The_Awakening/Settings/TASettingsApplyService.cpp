#include "Settings/TASettingsApplyService.h"
#include "Core/TALocalizeSubsystem.h"
#include "Story/TADialogueSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"

bool FTASettingsApplyService::Apply(ULocalPlayer* Player,const FTASettingDefinition& D,int32 V)
{
 const FName Handler=D.ApplyHandlerId;
 const FName Choice=D.GetChoiceValue(V);
 const float Number=D.Type==ETASettingType::Slider?D.ToSliderNumber(V):0;
 if (Handler==TEXT("Game.Language"))
 {
  auto* Loc=Player && Player->GetGameInstance()?Player->GetGameInstance()->GetSubsystem<UTALocalizeSubsystem>():nullptr;
  return Loc && Loc->SetLanguage(Choice.ToString());
 }
 if (Handler==TEXT("Game.DialogueTextSpeed"))
 {
  auto* Dialogue=Player && Player->GetGameInstance()?Player->GetGameInstance()->GetSubsystem<UTADialogueSubsystem>():nullptr;
  if (!Dialogue) return false; Dialogue->CharsPerSecond=Number; return true;
 }
 if (Handler==TEXT("Display.Brightness")) { if (!GEngine) return false; GEngine->DisplayGamma=Number; return true; }
 const FString Id=Handler.ToString();
 // Input consumers query stored settings at their existing, authorized ingress.
 if (Id.StartsWith(TEXT("MouseKeyboard.")) || Id.StartsWith(TEXT("Controller.")) || Handler==TEXT("Display.AdvancedData")) return true;
 auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
 if (Handler==TEXT("Display.WindowMode"))
 {
  U->SetFullscreenMode(Choice==TEXT("Fullscreen")?EWindowMode::Fullscreen:Choice==TEXT("WindowedFullscreen")?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);
  U->ApplyResolutionSettings(false); return true;
 }
 if (Handler==TEXT("Display.Resolution"))
 {
  FString W,H; int32 X=0,Y=0;
  if (!Choice.ToString().Split(TEXT("x"),&W,&H) || !LexTryParseString(X,*W) || !LexTryParseString(Y,*H) || X<=0 || Y<=0) return false;
  U->SetScreenResolution(FIntPoint(X,Y)); U->ApplyResolutionSettings(false); return true;
 }
 if (Handler==TEXT("Display.VSync")) U->SetVSyncEnabled(V==1);
 else if (Handler==TEXT("Display.FrameRateLimit"))
 {
  float N=0; if (Choice!=TEXT("Unlimited") && !LexTryParseString(N,*Choice.ToString())) return false;
  U->SetFrameRateLimit(N);
 }
 else return false;
 U->ApplyNonResolutionSettings(); return true;
}
bool FTASettingsApplyService::IsDisplaySetting(FName Id)
{
 return Id==TEXT("Display.WindowMode") || Id==TEXT("Display.Resolution") || Id==TEXT("Display.VSync") || Id==TEXT("Display.FrameRateLimit");
}
bool FTASettingsApplyService::ReadDisplayValue(const FTASettingDefinition& D,int32& V)
{
 const auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
 const FName Id=D.ApplyHandlerId; FName Choice;
 if (Id==TEXT("Display.VSync")) { V=U->IsVSyncEnabled()?1:0; return true; }
 if (Id==TEXT("Display.WindowMode")) Choice=U->GetFullscreenMode()==EWindowMode::Fullscreen?TEXT("Fullscreen"):U->GetFullscreenMode()==EWindowMode::WindowedFullscreen?TEXT("WindowedFullscreen"):TEXT("Windowed");
 else if (Id==TEXT("Display.Resolution")) { const auto R=U->GetScreenResolution(); Choice=FName(*FString::Printf(TEXT("%dx%d"),R.X,R.Y)); }
 else if (Id==TEXT("Display.FrameRateLimit")) Choice=U->GetFrameRateLimit()<=0?FName(TEXT("Unlimited")):FName(*FString::FromInt(FMath::RoundToInt(U->GetFrameRateLimit())));
 else return false;
 V=D.FindChoiceIndex(Choice); return V!=INDEX_NONE;
}
