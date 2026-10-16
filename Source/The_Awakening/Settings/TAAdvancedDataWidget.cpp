#include "Settings/TAAdvancedDataWidget.h"
#include "Settings/TASettingsSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"
#include "Core/TAVersionLibrary.h"
#include "Core/TATimeSubsystem.h"
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
FString UTAAdvancedDataWidget::GetPlatformText(const TFunctionRef<FString(const FString&)>& T)
{
 // 发行平台来自构建配置（DefaultEngine.ini [OnlineSubsystem] DefaultPlatformService），而不是运行时在线子系统是否初始化成功：
 // 编辑器里没开 Steam 客户端时在线子系统会退回 NULL，但这个版本的发行平台仍然是 Steam。
 FString Platform;
 GConfig->GetString(TEXT("OnlineSubsystem"),TEXT("DefaultPlatformService"),Platform,GEngineIni);
 if (Platform.IsEmpty()) return T(TEXT("Settings.Value.Unknown"));
 const FString Key=TEXT("Settings.Platform.")+Platform;
 const FString Localized=T(Key);
 return Localized==Key?Platform:Localized;
}
FString UTAAdvancedDataWidget::FormatGameTime(double Seconds,float Scale,const FText& Pattern)
{
 const int64 Total=FMath::FloorToInt64(FMath::Max(Seconds,0.0));
 const FString Clock=FString::Printf(TEXT("%02lld:%02lld:%02lld"),Total/3600,(Total/60)%60,Total%60);
 FNumberFormattingOptions Options; Options.MinimumFractionalDigits=0; Options.MaximumFractionalDigits=2; Options.UseGrouping=false;
 FFormatNamedArguments Args;
 Args.Add(TEXT("Time"),FText::FromString(Clock));
 Args.Add(TEXT("Scale"),FText::AsNumber(Scale,&Options));
 return FText::Format(FTextFormat(Pattern),Args).ToString();
}
FString UTAAdvancedDataWidget::GetGameTimeText(const UTALocalizeSubsystem* Loc) const
{
 const UTATimeSubsystem* Time=UTATimeSubsystem::Get(this);
 const FText Pattern=Loc?Loc->GetText(TEXT("Settings.AdvancedData.GameTime")):FText::FromString(TEXT("{Time}(x{Scale})"));
 return FormatGameTime(Time?Time->GetGameplayTime():0.0,Time?Time->GetGameplayTimeScale():1.f,Pattern);
}
void UTAAdvancedDataWidget::Refresh()
{
 if (!Text_AdvancedData) return;
 const FName Mode=Settings?Settings->GetChoice(TEXT("Display.AdvancedData")):NAME_None;
 if (Mode.IsNone() || Mode==TEXT("Off"))
 { Text_AdvancedData->SetText(FText::GetEmpty()); return; }
 const auto* GI=GetGameInstance(); const auto* Loc=GI?GI->GetSubsystem<UTALocalizeSubsystem>():nullptr;
 auto T=[&](const FString& Id){return Loc?Loc->GetText(Id).ToString():Id;};
 const auto* U=UGameUserSettings::GetGameUserSettings();
 const float Limit=U?U->GetFrameRateLimit():0;
 const int32 FPS=FMath::RoundToInt(Frames/FMath::Max(FPlatformTime::Seconds()-LastSample,0.001));
 FString S=FString::Printf(TEXT("%d（%s）FPS"),FPS,Limit>0?*FString::FromInt(FMath::RoundToInt(Limit)):*T(TEXT("Settings.Value.Unlimited")));
 if (Mode==TEXT("Full"))
 {
  const FString Version=UTAVersionLibrary::GetProjectVersion();
  const FString Build=UTAVersionLibrary::GetBuildId();
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
  S+=TEXT("-")+GetPlatformText(T)+TEXT("-")+GetGameTimeText(Loc);
 }
 Text_AdvancedData->SetText(FText::FromString(S));
}
