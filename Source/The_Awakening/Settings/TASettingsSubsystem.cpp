#include "Settings/TASettingsSubsystem.h"
#include "Settings/TASettingsApplyService.h"
#include "Core/TALocalizeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DefaultValueHelper.h"
#include "HAL/PlatformTime.h"

namespace { const TCHAR* Section=TEXT("TheAwakening.PlayerSettings"); }
void UTASettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Definitions=FTASettingsCatalog::Build(this);
 FString AssetPath;
 if (GConfig && GConfig->GetString(Section,TEXT("DefinitionAsset"),AssetPath,GGameIni))
  if (auto* Asset=LoadObject<UTASettingsDefinitionAsset>(nullptr,*AssetPath)) UseDefinitionAsset(Asset);
 LoadSavedValues();
 // Do not replace existing engine display settings with catalog defaults on startup.
 for (const auto& D:Definitions)
 {
  if (D.Type==ETASettingType::Submenu) continue;
  FTASettingValue V;
  if (!FTASettingsApplyService::ReadDisplayValue(D.ApplyHandlerId,V))
  {
   GetValue(D.SettingId,V); FTASettingsApplyService::Apply(GetLocalPlayer(),D.ApplyHandlerId,V);
  }
 }
}
void UTASettingsSubsystem::Deinitialize() { RevertVideoMode(); Super::Deinitialize(); }
bool UTASettingsSubsystem::UseDefinitionAsset(const UTASettingsDefinitionAsset* Asset)
{
 if (!Asset || Asset->Definitions.IsEmpty()) return false;
 auto NewDefinitions=Asset->Definitions; TSet<FName> Seen;
 for (auto& D:NewDefinitions)
 {
  FTASettingsCatalog::RefreshCandidates(D);
  if (!D.IsValidDefinition() || Seen.Contains(D.SettingId)) return false;
  Seen.Add(D.SettingId);
 }
 Definitions=MoveTemp(NewDefinitions);
 // Preserve live state; opening a widget never reloads saved values or applies defaults.
 for (auto It=CurrentValues.CreateIterator();It;++It)
 {
  const auto* D=FindDefinition(It.Key()); FTASettingValue N;
  if (!D || !D->Normalize(It.Value(),N)) It.RemoveCurrent(); else It.Value()=N;
 }
 return true;
}
const FTASettingDefinition* UTASettingsSubsystem::FindDefinition(FName Id) const
{ return Definitions.FindByPredicate([Id](const auto& D){return D.SettingId==Id;}); }
bool UTASettingsSubsystem::GetValue(FName Id,FTASettingValue& Out) const
{
 const auto* D=FindDefinition(Id); if (!D) return false;
 if (FTASettingsApplyService::ReadDisplayValue(D->ApplyHandlerId,Out)) return true;
 if (const auto* V=CurrentValues.Find(Id)) Out=*V; else Out=D->DefaultValue;
 return true;
}
bool UTASettingsSubsystem::SetValue(FName Id,const FTASettingValue& Value)
{
 const auto* D=FindDefinition(Id); FTASettingValue N;
 if (!D || !D->Normalize(Value,N)) return false;
 const FName Handler=D->ApplyHandlerId; const auto Policy=D->ApplyPolicy;
 if (Policy==ETASettingApplyPolicy::VideoMode && !HasPendingVideoMode())
 {
  auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
  PreviousResolution=U->GetScreenResolution(); PreviousWindowMode=U->GetFullscreenMode(); VideoDeadline=FPlatformTime::Seconds()+15;
 }
 if (!FTASettingsApplyService::Apply(GetLocalPlayer(),Handler,N)) { if (Policy==ETASettingApplyPolicy::VideoMode) RevertVideoMode(); return false; }
 CurrentValues.Add(Id,N);
 FTASettingValue Display;
 if (!HasPendingVideoMode() && FTASettingsApplyService::ReadDisplayValue(Handler,Display))
  if (auto* U=UGameUserSettings::GetGameUserSettings()) U->SaveSettings();
 SaveValues(); OnSettingValueChanged.Broadcast(Id); return true;
}
bool UTASettingsSubsystem::AdjustValue(FName Id,int32 Direction)
{
 const auto* D=FindDefinition(Id); FTASettingValue V;
 if (!D || !Direction || !GetValue(Id,V)) return false;
 if (D->Type==ETASettingType::Toggle) V.bBoolean=!V.bBoolean;
 else if (D->Type==ETASettingType::Slider) V.Number+=D->Step*(Direction<0?-1:1);
 else if (D->Type==ETASettingType::Choice && !D->Choices.IsEmpty())
 {
  int32 I=D->Choices.IndexOfByPredicate([&](const auto& C){return C.Value==V.Choice;});
  I=I==INDEX_NONE?0:(I+(Direction<0?-1:1)+D->Choices.Num())%D->Choices.Num(); V.Choice=D->Choices[I].Value;
 }
 else return false;
 return SetValue(Id,V);
}
void UTASettingsSubsystem::SetFavorite(FName Id,bool B)
{
 const auto* D=FindDefinition(Id); if (!D || D->bSubmenuOnly || !D->bCanFavorite) return;
 if (B) FavoriteSettingIds.Add(Id); else FavoriteSettingIds.Remove(Id);
 SaveValues(); OnFavoritesChanged.Broadcast();
}
void UTASettingsSubsystem::RestoreSettingDefault(FName Id) { if (const auto* D=FindDefinition(Id)) SetValue(Id,D->DefaultValue); }
void UTASettingsSubsystem::RestoreDefaults()
{
 // Copy IDs: language changes may synchronously refresh widgets.
 TArray<FName> Ids; for (const auto& D:Definitions) if (D.Type!=ETASettingType::Submenu) Ids.Add(D.SettingId);
 for (FName Id:Ids) RestoreSettingDefault(Id);
}
void UTASettingsSubsystem::ConfirmVideoMode()
{
 if (!HasPendingVideoMode()) return; VideoDeadline=0;
 if (auto* U=UGameUserSettings::GetGameUserSettings()) { U->ConfirmVideoMode(); U->SaveSettings(); }
 NotifyVideoValues();
}
void UTASettingsSubsystem::RevertVideoMode()
{
 if (!HasPendingVideoMode()) return; VideoDeadline=0;
 if (auto* U=UGameUserSettings::GetGameUserSettings())
 { U->SetScreenResolution(PreviousResolution); U->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousWindowMode)); U->ApplyResolutionSettings(false); U->SaveSettings(); }
 NotifyVideoValues();
}
void UTASettingsSubsystem::NotifyVideoValues()
{ OnSettingValueChanged.Broadcast(TEXT("Display.WindowMode")); OnSettingValueChanged.Broadcast(TEXT("Display.Resolution")); }
void UTASettingsSubsystem::CheckVideoModeTimeout() { if (HasPendingVideoMode() && FPlatformTime::Seconds()>=VideoDeadline) RevertVideoMode(); }
float UTASettingsSubsystem::GetNumber(FName Id,float Fallback) const
{ FTASettingValue V; return GetValue(Id,V) && V.Kind==ETASettingValueKind::Number ? V.Number:Fallback; }
bool UTASettingsSubsystem::GetBoolean(FName Id,bool Fallback) const
{ FTASettingValue V; return GetValue(Id,V) && V.Kind==ETASettingValueKind::Boolean ? V.bBoolean:Fallback; }
FText UTASettingsSubsystem::FormatValue(const FTASettingDefinition& D) const
{
 FTASettingValue V; if (!GetValue(D.SettingId,V)) return FText::GetEmpty();
 const auto* GI=GetLocalPlayer()?GetLocalPlayer()->GetGameInstance():nullptr; const auto* Loc=GI?GI->GetSubsystem<UTALocalizeSubsystem>():nullptr;
 auto Text=[Loc](const FString& Id){return Loc?Loc->GetText(Id):FText::FromString(Id);};
 if (D.Type==ETASettingType::Toggle) return Text(V.bBoolean?TEXT("Settings.Value.On"):TEXT("Settings.Value.Off"));
 if (D.Type==ETASettingType::Choice)
 {
  const auto* C=D.Choices.FindByPredicate([&](const auto& X){return X.Value==V.Choice;});
  return C && !C->NameTextId.IsEmpty()?Text(C->NameTextId):FText::FromName(V.Choice);
 }
 if (D.Type!=ETASettingType::Slider) return FText::GetEmpty();
 FNumberFormattingOptions Options; Options.MinimumFractionalDigits=D.DisplayFormat==ETASettingDisplayFormat::Integer?0:D.DecimalPlaces;
 Options.MaximumFractionalDigits=Options.MinimumFractionalDigits;
 FText N=D.DisplayFormat==ETASettingDisplayFormat::Percent ? FText::AsPercent(V.Number,&Options):FText::AsNumber(V.Number,&Options);
 if (D.DisplayFormat==ETASettingDisplayFormat::Multiplier) N=FText::Format(Text(TEXT("Settings.Format.Multiplier")),N);
 if (!D.UnitTextId.IsEmpty()) N=FText::Format(Text(TEXT("Settings.Format.Unit")),N,Text(D.UnitTextId));
 return N;
}
void UTASettingsSubsystem::LoadSavedValues()
{
 if (!GConfig) return;
 for (const auto& D:Definitions)
 {
  FTASettingValue Display; if (D.Type==ETASettingType::Submenu || FTASettingsApplyService::ReadDisplayValue(D.ApplyHandlerId,Display)) continue;
  FString Encoded; if (!GConfig->GetString(Section,*D.SettingId.ToString(),Encoded,GGameUserSettingsIni)) continue;
  FTASettingValue V=D.DefaultValue,N;
  if (V.Kind==ETASettingValueKind::Boolean) { if (Encoded!=TEXT("0") && Encoded!=TEXT("1")) continue; V.bBoolean=Encoded==TEXT("1"); }
  else if (V.Kind==ETASettingValueKind::Number) { if (!FDefaultValueHelper::ParseFloat(Encoded,V.Number)) continue; }
  else if (V.Kind==ETASettingValueKind::Choice) V.Choice=FName(*Encoded);
  if (D.Normalize(V,N)) CurrentValues.Add(D.SettingId,N);
 }
 TArray<FString> Favorites; GConfig->GetArray(Section,TEXT("Favorites"),Favorites,GGameUserSettingsIni);
 for (const FString& S:Favorites) if (const auto* D=FindDefinition(FName(*S));D && !D->bSubmenuOnly) FavoriteSettingIds.Add(D->SettingId);
}
void UTASettingsSubsystem::SaveValues() const
{
 if (!GConfig) return;
 GConfig->SetInt(Section,TEXT("SchemaVersion"),1,GGameUserSettingsIni);
 for (const auto& D:Definitions)
 {
  FTASettingValue Display; if (D.Type==ETASettingType::Submenu || FTASettingsApplyService::ReadDisplayValue(D.ApplyHandlerId,Display)) continue;
  const auto* V=CurrentValues.Find(D.SettingId); if (!V) continue;
  FString S=V->Kind==ETASettingValueKind::Boolean?(V->bBoolean?TEXT("1"):TEXT("0")):V->Kind==ETASettingValueKind::Number?FString::SanitizeFloat(V->Number):V->Choice.ToString();
  GConfig->SetString(Section,*D.SettingId.ToString(),*S,GGameUserSettingsIni);
 }
 TArray<FString> Favorites; for(FName Id:FavoriteSettingIds) Favorites.Add(Id.ToString()); Favorites.Sort();
 GConfig->SetArray(Section,TEXT("Favorites"),Favorites,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni);
}
