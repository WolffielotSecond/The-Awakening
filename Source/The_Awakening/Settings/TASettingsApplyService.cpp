#include "Settings/TASettingsApplyService.h"
#include "Settings/TADisplaySettingsService.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Story/TADialogueSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "The_AwakeningCharacter.h"
#include "GameFramework/PlayerController.h"

bool FTASettingsApplyService::Apply(ULocalPlayer* Player,const FTASettingDefinition& D,int32 V)
{
 const FName Handler=D.ApplyHandlerId;
 const FName Choice=D.GetChoiceValue(V);
 const float Number=D.Type==ETASettingType::Slider?D.ToSliderNumber(V):0;
 if (Handler==TEXT("Game.ToggleSprint"))
 {
  if (APlayerController* PC=Player?Player->PlayerController.Get():nullptr)
   if (auto* Character=Cast<AThe_AwakeningCharacter>(PC->GetPawn())) Character->SetToggleSprintEnabled(V==1);
  return true;
 }
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
 if (Handler==TEXT("Controller.IconStyle"))
 {
  auto* Icons=Player && Player->GetGameInstance()?Player->GetGameInstance()->GetSubsystem<UTAInputIconSubsystem>():nullptr;
  if (!Icons) return false;
  Icons->SetGamepadIconStyle(Choice==TEXT("Xbox")?ETAGamepadIconStyle::Xbox:Choice==TEXT("DualSense")?ETAGamepadIconStyle::DualSense:ETAGamepadIconStyle::Auto);
  return true;
 }
 const FString Id=Handler.ToString();
 // Input consumers query stored settings at their existing, authorized ingress.
 if (Id.StartsWith(TEXT("MouseKeyboard.")) || Id.StartsWith(TEXT("Controller.")) || Handler==TEXT("Display.AdvancedData")) return true;
 if (FTADisplaySettingsService::IsVideoSetting(Handler)) return FTADisplaySettingsService::ApplyChange(Handler,Choice);
 auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
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
 return FTADisplaySettingsService::IsVideoSetting(Id) || Id==TEXT("Display.VSync") || Id==TEXT("Display.FrameRateLimit");
}
bool FTASettingsApplyService::ReadDisplayValue(const FTASettingDefinition& D,int32& V)
{
 const auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
 const FName Id=D.ApplyHandlerId; FName Choice;
 if (FTADisplaySettingsService::IsVideoSetting(Id))
 {
  const auto Monitors=FTADisplaySettingsService::GetMonitors();
  Choice=FTADisplaySettingsService::ReadChoice(Id,Monitors,FTADisplaySettingsService::GetCurrentState(Monitors));
  V=D.FindChoiceIndex(Choice); return V!=INDEX_NONE;
 }
 if (Id==TEXT("Display.VSync")) { V=U->IsVSyncEnabled()?1:0; return true; }
 if (Id==TEXT("Display.FrameRateLimit")) Choice=U->GetFrameRateLimit()<=0?FName(TEXT("Unlimited")):FName(*FString::FromInt(FMath::RoundToInt(U->GetFrameRateLimit())));
 else return false;
 V=D.FindChoiceIndex(Choice); return V!=INDEX_NONE;
}
