#include "Settings/TAAdvancedDataWidget.h"
#include "Settings/TASettingsSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"

void UTAAdvancedDataWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (GetOwningLocalPlayer()) Settings=GetOwningLocalPlayer()->GetSubsystem<UTASettingsSubsystem>();
 LastSample=FPlatformTime::Seconds(); Frames=0;
 if (Text_AdvancedData) Text_AdvancedData->SetText(FText::GetEmpty());
}
void UTAAdvancedDataWidget::NativeTick(const FGeometry& G,float Delta)
{
 Super::NativeTick(G,Delta); ++Frames;
 const double Now=FPlatformTime::Seconds();
 if (Now-LastSample>=FMath::Max(RefreshInterval,0.1f)) { Refresh(); Frames=0; LastSample=Now; }
}
void UTAAdvancedDataWidget::Refresh()
{
 if (!Text_AdvancedData) return;
 FTASettingValue V; if (!Settings || !Settings->GetValue(TEXT("Display.AdvancedData"),V) || V.Choice==TEXT("Off"))
 { Text_AdvancedData->SetText(FText::GetEmpty()); return; }
 const auto* GI=GetGameInstance(); const auto* Loc=GI?GI->GetSubsystem<UTALocalizeSubsystem>():nullptr;
 auto T=[&](const FString& Id){return Loc?Loc->GetText(Id).ToString():Id;};
 const auto* U=UGameUserSettings::GetGameUserSettings();
 const float Limit=U?U->GetFrameRateLimit():0;
 const int32 FPS=FMath::RoundToInt(Frames/FMath::Max(FPlatformTime::Seconds()-LastSample,0.001));
 FString S=FString::Printf(TEXT("%d（%s）FPS"),FPS,Limit>0?*FString::FromInt(FMath::RoundToInt(Limit)):*T(TEXT("Settings.Value.Unlimited")));
 if (V.Choice==TEXT("Full"))
 {
  FString Version,Build;
  if (GConfig) { GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),TEXT("ProjectVersion"),Version,GGameIni); GConfig->GetString(TEXT("TheAwakening.Build"),TEXT("BuildId"),Build,GGameIni); }
  if (Version.IsEmpty()) Version=T(TEXT("Settings.Value.Unknown"));
  if (Build.IsEmpty()) Build=T(TEXT("Settings.Value.Unknown"));
  int32 Quality=U?U->GetViewDistanceQuality():-1;
  if (U && (U->GetShadowQuality()!=Quality || U->GetGlobalIlluminationQuality()!=Quality || U->GetReflectionQuality()!=Quality ||
   U->GetAntiAliasingQuality()!=Quality || U->GetTextureQuality()!=Quality || U->GetVisualEffectQuality()!=Quality ||
   U->GetPostProcessingQuality()!=Quality || U->GetFoliageQuality()!=Quality || U->GetShadingQuality()!=Quality)) Quality=-1;
  const TCHAR* QualityNames[]={TEXT("Low"),TEXT("Medium"),TEXT("High"),TEXT("Epic"),TEXT("Cinematic")};
  const FString Q=T(Quality>=0 && Quality<5?FString(TEXT("Settings.Quality."))+QualityNames[Quality]:TEXT("Settings.Quality.Custom"));
  FString Language=Loc?T(TEXT("Settings.Value.")+Loc->GetCurrentLanguage()):T(TEXT("Settings.Value.Unknown"));
  const auto* Icons=GI?GI->GetSubsystem<UTAInputIconSubsystem>():nullptr;
  const EInputDeviceType Device=Icons?Icons->GetCurrentDeviceType():EInputDeviceType::KeyboardMouse;
  const TCHAR* DeviceNames[]={TEXT("KeyboardMouse"),TEXT("Xbox"),TEXT("PS5"),TEXT("Switch")};
  const FString DeviceText=T(FString(TEXT("Settings.Device."))+DeviceNames[static_cast<uint8>(Device)]);
  S+=FString::Printf(TEXT("-%s-%s-%s-%s-%s"),*Version,*Build,*Q,*Language,*DeviceText);
 }
 Text_AdvancedData->SetText(FText::FromString(S));
}
