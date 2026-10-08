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
 Pages=FTASettingsCatalog::BuildPages();
 Definitions=FTASettingsCatalog::Build(this);
 FString AssetPath;
 if (GConfig && GConfig->GetString(Section,TEXT("DefinitionAsset"),AssetPath,GGameIni))
  if (auto* Asset=LoadObject<UTASettingsDefinitionAsset>(nullptr,*AssetPath)) UseDefinitionAsset(Asset);
 LoadSavedValues();
 // Do not replace existing engine display settings with catalog defaults on startup.
 for (const auto& D:Definitions)
 {
  if (D.Type==ETASettingType::Submenu) continue;
  int32 V=0;
  if (!FTASettingsApplyService::IsDisplaySetting(D.ApplyHandlerId))
  {
   GetValue(D.SettingId,V); FTASettingsApplyService::Apply(GetLocalPlayer(),D,V);
  }
 }
 SaveValues();
}
void UTASettingsSubsystem::Deinitialize() { RevertVideoMode(); Super::Deinitialize(); }
bool UTASettingsSubsystem::UseDefinitionAsset(const UTASettingsDefinitionAsset* Asset)
{
 if (!Asset) return false;
 auto NewPages=Asset->PageDefinitionAsset?Asset->PageDefinitionAsset->Pages:FTASettingsCatalog::BuildPages();
 FString Error;
 if (!FTASettingsCatalog::ValidatePages(NewPages,Error)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return false; }
 auto NewDefinitions=Asset->Definitions; TSet<FName> Seen;
 for (auto& D:NewDefinitions)
 {
  FTASettingsCatalog::RefreshCandidates(D);
  if (!D.IsValidDefinition() || !FTASettingsCatalog::ValidateLocations(D,NewPages) || Seen.Contains(D.SettingId))
  { UE_LOG(LogTemp,Warning,TEXT("Rejected settings asset: invalid definition, duplicate ID or unresolved page/section for %s"),*D.SettingId.ToString()); return false; }
  Seen.Add(D.SettingId);
 }
 Pages=MoveTemp(NewPages); Definitions=MoveTemp(NewDefinitions);
 // Preserve live state; opening a widget never reloads saved values or applies defaults.
 for (auto It=CurrentValues.CreateIterator();It;++It)
 {
  const auto* D=FindDefinition(It.Key()); int32 N=0;
  if (!D || !D->Normalize(It.Value(),N)) It.RemoveCurrent(); else It.Value()=N;
 }
 return true;
}
const FTASettingDefinition* UTASettingsSubsystem::FindDefinition(FName Id) const
{ return Definitions.FindByPredicate([Id](const auto& D){return D.SettingId==Id;}); }
bool UTASettingsSubsystem::GetValue(FName Id,int32& Out) const
{
 const auto* D=FindDefinition(Id); if (!D || D->Type==ETASettingType::Submenu) return false;
 if (FTASettingsApplyService::ReadDisplayValue(*D,Out)) return true;
 if (const auto* V=CurrentValues.Find(Id)) Out=*V; else Out=D->DefaultValue;
 return true;
}
bool UTASettingsSubsystem::SetValue(FName Id,int32 Value)
{
 const auto* D=FindDefinition(Id); int32 N=0;
 if (!D || !D->Normalize(Value,N)) return false;
 const auto Policy=D->ApplyPolicy;
 if (Policy==ETASettingApplyPolicy::VideoMode && !HasPendingVideoMode())
 {
  auto* U=UGameUserSettings::GetGameUserSettings(); if (!U) return false;
  PreviousResolution=U->GetScreenResolution(); PreviousWindowMode=U->GetFullscreenMode(); VideoDeadline=FPlatformTime::Seconds()+15;
 }
 if (!FTASettingsApplyService::Apply(GetLocalPlayer(),*D,N)) { if (Policy==ETASettingApplyPolicy::VideoMode) RevertVideoMode(); return false; }
 CurrentValues.Add(Id,N);
 int32 Display=0;
 if (!HasPendingVideoMode() && FTASettingsApplyService::ReadDisplayValue(*D,Display))
  if (auto* U=UGameUserSettings::GetGameUserSettings()) U->SaveSettings();
 SaveValues(); OnSettingValueChanged.Broadcast(Id); return true;
}
bool UTASettingsSubsystem::AdjustValue(FName Id,int32 Direction)
{
 const auto* D=FindDefinition(Id); int32 V=0;
 if (!D || !Direction || !GetValue(Id,V)) return false;
 if (D->Type==ETASettingType::Toggle) V=V==0?1:0;
 else if (D->Type==ETASettingType::Slider) V+=D->Step*(Direction<0?-1:1);
 else if (D->Type==ETASettingType::Choice && !D->Choices.IsEmpty())
 {
  V=(V+(Direction<0?-1:1)+D->Choices.Num())%D->Choices.Num();
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
 SaveValues(); NotifyVideoValues();
}
void UTASettingsSubsystem::RevertVideoMode()
{
 if (!HasPendingVideoMode()) return; VideoDeadline=0;
 if (auto* U=UGameUserSettings::GetGameUserSettings())
 { U->SetScreenResolution(PreviousResolution); U->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousWindowMode)); U->ApplyResolutionSettings(false); U->SaveSettings(); }
 SaveValues(); NotifyVideoValues();
}
void UTASettingsSubsystem::NotifyVideoValues()
{ OnSettingValueChanged.Broadcast(TEXT("Display.WindowMode")); OnSettingValueChanged.Broadcast(TEXT("Display.Resolution")); }
void UTASettingsSubsystem::CheckVideoModeTimeout() { if (HasPendingVideoMode() && FPlatformTime::Seconds()>=VideoDeadline) RevertVideoMode(); }
int32 UTASettingsSubsystem::GetInteger(FName Id,int32 Fallback) const
{ int32 V; return GetValue(Id,V)?V:Fallback; }
float UTASettingsSubsystem::GetNumber(FName Id,float Fallback) const
{
 const auto* D=FindDefinition(Id); int32 V;
 return D && D->Type==ETASettingType::Slider && GetValue(Id,V)?D->ToSliderNumber(V):Fallback;
}
bool UTASettingsSubsystem::GetBoolean(FName Id,bool Fallback) const
{
 const auto* D=FindDefinition(Id); int32 V;
 return D && D->Type==ETASettingType::Toggle && GetValue(Id,V)?V==1:Fallback;
}
FName UTASettingsSubsystem::GetChoice(FName Id,FName Fallback) const
{
 const auto* D=FindDefinition(Id); int32 V;
 return D && D->Type==ETASettingType::Choice && GetValue(Id,V)?D->GetChoiceValue(V):Fallback;
}
FText UTASettingsSubsystem::FormatValue(const FTASettingDefinition& D) const
{
 int32 V=0; if (!GetValue(D.SettingId,V)) return FText::GetEmpty();
 const auto* GI=GetLocalPlayer()?GetLocalPlayer()->GetGameInstance():nullptr; const auto* Loc=GI?GI->GetSubsystem<UTALocalizeSubsystem>():nullptr;
 auto Text=[Loc](const FString& Id){return Loc?Loc->GetText(Id):FText::FromString(Id);};
 if (D.Type==ETASettingType::Toggle) return Text(V==1?TEXT("Settings.Value.On"):TEXT("Settings.Value.Off"));
 if (D.Type==ETASettingType::Choice)
 {
  if (!D.Choices.IsValidIndex(V)) return FText::GetEmpty();
  const auto& C=D.Choices[V];
  return !C.NameTextId.IsEmpty()?Text(C.NameTextId):FText::FromName(C.Value);
 }
 if (D.Type!=ETASettingType::Slider) return FText::GetEmpty();
 FNumberFormattingOptions Options; Options.MinimumFractionalDigits=D.DisplayFormat==ETASettingDisplayFormat::Integer?0:D.DecimalPlaces;
 Options.MaximumFractionalDigits=Options.MinimumFractionalDigits;
 FText N=D.DisplayFormat==ETASettingDisplayFormat::Percent ? FText::AsPercent(D.ToSliderNumber(V),&Options):FText::AsNumber(D.ToSliderNumber(V),&Options);
 if (D.DisplayFormat==ETASettingDisplayFormat::Multiplier) N=FText::Format(Text(TEXT("Settings.Format.Multiplier")),N);
 if (!D.UnitTextId.IsEmpty()) N=FText::Format(Text(TEXT("Settings.Format.Unit")),N,Text(D.UnitTextId));
 return N;
}
void UTASettingsSubsystem::LoadSavedValues()
{
 if (!GConfig) return;
 int32 Schema=1; GConfig->GetInt(Section,TEXT("SchemaVersion"),Schema,GGameUserSettingsIni);
 for (const auto& D:Definitions)
 {
  // Engine display settings retain their engine authority across hardware changes.
  if (D.Type==ETASettingType::Submenu || FTASettingsApplyService::IsDisplaySetting(D.ApplyHandlerId)) continue;
  FString Encoded; if (!GConfig->GetString(Section,*D.SettingId.ToString(),Encoded,GGameUserSettingsIni)) continue;
  int32 V=0,N=0;
  if (Schema>=2)
  { if (!FDefaultValueHelper::ParseInt(Encoded,V)) continue; }
  else if (D.Type==ETASettingType::Choice) V=D.FindChoiceIndex(FName(*Encoded));
  else if (D.Type==ETASettingType::Slider)
  {
   float Number=0; if (!FDefaultValueHelper::ParseFloat(Encoded,Number) || !FMath::IsFinite(Number)) continue;
   V=D.FromSliderNumber(Number);
  }
  else if (!FDefaultValueHelper::ParseInt(Encoded,V)) continue;
  if (D.Normalize(V,N)) CurrentValues.Add(D.SettingId,N);
 }
 TArray<FString> Favorites; GConfig->GetArray(Section,TEXT("Favorites"),Favorites,GGameUserSettingsIni);
 for (const FString& S:Favorites)
  if (const auto* D=FindDefinition(FName(*S));D && !D->bSubmenuOnly && D->bCanFavorite) FavoriteSettingIds.Add(D->SettingId);
}
void UTASettingsSubsystem::SaveValues() const
{
 if (!GConfig) return;
 GConfig->SetInt(Section,TEXT("SchemaVersion"),2,GGameUserSettingsIni);
 for (const auto& D:Definitions)
 {
  if (D.Type==ETASettingType::Submenu || (HasPendingVideoMode() && D.ApplyPolicy==ETASettingApplyPolicy::VideoMode)) continue;
  int32 V=0; if (!GetValue(D.SettingId,V)) continue;
  GConfig->SetInt(Section,*D.SettingId.ToString(),V,GGameUserSettingsIni);
 }
 TArray<FString> Favorites; for(FName Id:FavoriteSettingIds) Favorites.Add(Id.ToString()); Favorites.Sort();
 GConfig->SetArray(Section,TEXT("Favorites"),Favorites,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni);
}
