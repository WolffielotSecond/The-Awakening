#include "Settings/TASettingsApplyService.h"
#include "Core/TALocalizeSubsystem.h"
#include "Story/TADialogueSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"

bool FTASettingsApplyService::Apply(ULocalPlayer* Player,FName Handler,const FTASettingValue& V)
{
 if (Handler==TEXT("Game.Language"))
 {
  auto* Loc=Player && Player->GetGameInstance()?Player->GetGameInstance()->GetSubsystem<UTALocalizeSubsystem>():nullptr;
  return Loc && Loc->SetLanguage(V.Choice.ToString());
 }
 if (Handler==TEXT("Game.DialogueTextSpeed"))
 {
  auto* Dialogue=Player && Player->GetGameInstance()?Player->GetGameInstance()->GetSubsystem<UTADialogueSubsystem>():nullptr;
  if (!Dialogue) return false; Dialogue->CharsPerSecond=V.Number; return true;
 }
 if (Handler==TEXT("Display.Brightness")) { if (!GEngine) return false; GEngine->DisplayGamma=V.Number; return true; }
 const FString Id=Handler.ToString();
 // Input consumers query stored settings at their existing, authorized ingress.
 if (Id.StartsWith(TEXT("MouseKeyboard.")) || Id.StartsWith(TEXT("Controller.")) || Handler==TEXT("Display.AdvancedData")) return true;
 auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
 if (Handler==TEXT("Display.WindowMode"))
 {
  U->SetFullscreenMode(V.Choice==TEXT("Fullscreen")?EWindowMode::Fullscreen:V.Choice==TEXT("WindowedFullscreen")?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);
  U->ApplyResolutionSettings(false); return true;
 }
 if (Handler==TEXT("Display.Resolution"))
 {
  FString W,H; int32 X=0,Y=0;
  if (!V.Choice.ToString().Split(TEXT("x"),&W,&H) || !LexTryParseString(X,*W) || !LexTryParseString(Y,*H) || X<=0 || Y<=0) return false;
  U->SetScreenResolution(FIntPoint(X,Y)); U->ApplyResolutionSettings(false); return true;
 }
 if (Handler==TEXT("Display.VSync")) U->SetVSyncEnabled(V.bBoolean);
 else if (Handler==TEXT("Display.FrameRateLimit"))
 {
  float N=0; if (V.Choice!=TEXT("Unlimited") && !LexTryParseString(N,*V.Choice.ToString())) return false;
  U->SetFrameRateLimit(N);
 }
 else return false;
 U->ApplyNonResolutionSettings(); return true;
}
bool FTASettingsApplyService::ReadDisplayValue(FName Id,FTASettingValue& V)
{
 const auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
 if (Id==TEXT("Display.WindowMode")) V=FTASettingValue::ChoiceValue(U->GetFullscreenMode()==EWindowMode::Fullscreen?TEXT("Fullscreen"):U->GetFullscreenMode()==EWindowMode::WindowedFullscreen?TEXT("WindowedFullscreen"):TEXT("Windowed"));
 else if (Id==TEXT("Display.Resolution")) { const auto R=U->GetScreenResolution(); V=FTASettingValue::ChoiceValue(FName(*FString::Printf(TEXT("%dx%d"),R.X,R.Y))); }
 else if (Id==TEXT("Display.VSync")) V=FTASettingValue::Boolean(U->IsVSyncEnabled());
 else if (Id==TEXT("Display.FrameRateLimit")) V=FTASettingValue::ChoiceValue(U->GetFrameRateLimit()<=0?FName(TEXT("Unlimited")):FName(*FString::FromInt(FMath::RoundToInt(U->GetFrameRateLimit()))));
 else return false; return true;
}
