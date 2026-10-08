#include "Settings/TASettingsTypes.h"
#include "Settings/TASettingsMenuWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformMisc.h"
#include "Algo/StableSort.h"
#include "GameFramework/GameUserSettings.h"

FName FTASettingsCatalog::FavoritesPageId() { return TEXT("Settings.Page.Favorites"); }
TArray<FTASettingsPageDefinition> FTASettingsCatalog::BuildPages()
{
 TArray<FTASettingsPageDefinition> Pages;
 auto Add=[&](const TCHAR* Id,std::initializer_list<const TCHAR*> Sections)
 {
  FTASettingsPageDefinition D; D.PageId=Id; D.SortOrder=Pages.Num()*10;
  for (const TCHAR* Section:Sections) { FTASettingsSectionDefinition S; S.SectionId=Section; S.SortOrder=D.Sections.Num()*10; D.Sections.Add(S); }
  Pages.Add(D);
 };
 Add(TEXT("Settings.Page.Game"),{TEXT("Settings.Section.Game.General")});
 Add(TEXT("Settings.Page.Display"),{TEXT("Settings.Section.Display.Video"),TEXT("Settings.Section.Display.Information")});
 Add(TEXT("Settings.Page.Audio"),{TEXT("Settings.Section.Audio.Volume")});
 Add(TEXT("Settings.Page.MouseKeyboard"),{TEXT("Settings.Section.MouseKeyboard.Camera"),TEXT("Settings.Section.MouseKeyboard.Bindings")});
 Add(TEXT("Settings.Page.Controller"),{TEXT("Settings.Section.Controller.Camera"),TEXT("Settings.Section.Controller.Bindings"),TEXT("Settings.Section.Controller.Cursor")});
 return Pages;
}
void FTASettingsCatalog::SortPages(TArray<FTASettingsPageDefinition>& Pages)
{
 Algo::StableSort(Pages,[](const auto& A,const auto& B){return A.SortOrder<B.SortOrder;});
 for (auto& P:Pages) Algo::StableSort(P.Sections,[](const auto& A,const auto& B){return A.SortOrder<B.SortOrder;});
}
bool FTASettingsCatalog::ValidatePages(const TArray<FTASettingsPageDefinition>& Pages,FString& Error)
{
 TSet<FName> Seen;
 for (const auto& P:Pages)
 {
  if (P.PageId.IsNone() || P.PageId==FavoritesPageId() || P.PageId==TEXT("Favorites") || Seen.Contains(P.PageId))
  { Error=FString::Printf(TEXT("Invalid, reserved or duplicate PageId: %s"),*P.PageId.ToString()); return false; }
  Seen.Add(P.PageId); TSet<FName> Sections;
  for (const auto& S:P.Sections)
  {
   if (S.SectionId.IsNone() || Sections.Contains(S.SectionId))
   { Error=FString::Printf(TEXT("Invalid or duplicate SectionId in %s"),*P.PageId.ToString()); return false; }
   Sections.Add(S.SectionId);
  }
 }
 return true;
}
bool FTASettingsCatalog::ValidateLocations(const FTASettingDefinition& D,const TArray<FTASettingsPageDefinition>& Pages)
{
 TArray<FTASettingLocation> Seen;
 for (const auto& L:D.Locations.Items)
 {
  const auto* P=Pages.FindByPredicate([&](const auto& Page){return Page.PageId==L.PageId;});
  if (!P || Seen.Contains(L) || !P->Sections.ContainsByPredicate([&](const auto& S){return S.SectionId==L.SectionId;})) return false;
  Seen.Add(L);
 }
 return true;
}
TArray<FTASettingsPageDefinition> FTASettingsCatalog::BuildNavigationPages(const TArray<FTASettingsPageDefinition>& Pages)
{
 auto Result=Pages; SortPages(Result);
 FTASettingsPageDefinition Favorites; Favorites.PageId=FavoritesPageId(); Result.Insert(Favorites,0); return Result;
}
bool FTASettingDefinition::BelongsTo(FName PageId,FName SectionId) const
{
 return Locations.Items.ContainsByPredicate([&](const auto& L){return L.PageId==PageId && (SectionId.IsNone() || L.SectionId==SectionId);});
}
TArray<FTASettingsViewGroup> FTASettingsCatalog::BuildViewGroups(const TArray<FTASettingsPageDefinition>& Pages,const TArray<FTASettingDefinition>& Definitions,FName PageId,const TSet<FName>& Favorites)
{
 auto Ordered=Pages; SortPages(Ordered); TArray<FTASettingsViewGroup> Groups;
 if (PageId==FavoritesPageId())
 {
  for (const auto& P:Ordered)
  {
   FTASettingsViewGroup G; G.HeadingId=P.PageId;
   for (const auto& D:Definitions) if (!D.bSubmenuOnly && D.bCanFavorite && Favorites.Contains(D.SettingId) && D.BelongsTo(P.PageId)) G.SettingIds.Add(D.SettingId);
   if (!G.SettingIds.IsEmpty()) Groups.Add(MoveTemp(G));
  }
 }
 else if (const auto* P=Ordered.FindByPredicate([&](const auto& D){return D.PageId==PageId;}))
 {
  for (const auto& S:P->Sections)
  {
   FTASettingsViewGroup G; G.HeadingId=S.SectionId;
   for (const auto& D:Definitions) if (!D.bSubmenuOnly && D.BelongsTo(P->PageId,S.SectionId)) G.SettingIds.Add(D.SettingId);
   if (!G.SettingIds.IsEmpty()) Groups.Add(MoveTemp(G));
  }
 }
 return Groups;
}
void UTASettingsPageDefinitionAsset::PopulateDefaultPages()
{ Modify(); Pages=FTASettingsCatalog::BuildPages(); MarkPackageDirty(); }
void UTASettingsDefinitionAsset::PopulateDefaultDefinitions()
{ Modify(); Definitions=FTASettingsCatalog::Build(this); MarkPackageDirty(); }

bool FTASettingDefinition::Normalize(int32 Input,int32& Output) const
{
 if (Type==ETASettingType::Submenu) return false;
 if (Type==ETASettingType::Toggle && Input!=0 && Input!=1) return false;
 if (Type==ETASettingType::Choice && !Choices.IsValidIndex(Input)) return false;
 Output=Type==ETASettingType::Slider?FMath::Clamp(Input,1,100):Input;
 return true;
}
float FTASettingDefinition::ToSliderNumber(int32 StoredValue) const
{
 const int32 Position=FMath::Clamp(StoredValue,1,100);
 if (Position==1) return Minimum;
 if (Position==100) return Maximum;
 float N=FMath::Lerp(Minimum,Maximum,(Position-1)/99.f);
 if (PhysicalStep>0) N=Minimum+FMath::RoundToFloat((N-Minimum)/PhysicalStep)*PhysicalStep;
 return FMath::Clamp(N,Minimum,Maximum);
}
int32 FTASettingDefinition::FromSliderNumber(float Number) const
{
 if (!FMath::IsFinite(Number) || Maximum<=Minimum) return 1;
 return FMath::Clamp(1+FMath::RoundToInt((FMath::Clamp(Number,Minimum,Maximum)-Minimum)/(Maximum-Minimum)*99.f),1,100);
}
FName FTASettingDefinition::GetChoiceValue(int32 Index) const
{ return Choices.IsValidIndex(Index)?Choices[Index].Value:NAME_None; }
int32 FTASettingDefinition::FindChoiceIndex(FName Value) const
{ return Choices.IndexOfByPredicate([&](const auto& C){return C.Value==Value;}); }
bool FTASettingDefinition::IsValidDefinition() const
{
 if (SettingId.IsNone() || NameTextId.IsEmpty() || DescriptionTextId.IsEmpty()) return false;
 // Built-in submenu metadata can be edited before its WBP is assigned.
 if (Type==ETASettingType::Submenu) return TargetMenuWidgetClass || (TargetMenuDefinition && !TargetMenuDefinition->MenuId.IsNone());
 if (Type==ETASettingType::Slider && (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Maximum<=Minimum ||
  Step<1 || Step>99 || !FMath::IsFinite(PhysicalStep) || PhysicalStep<0 || DefaultValue<1 || DefaultValue>100)) return false;
 if (Type==ETASettingType::Choice)
 {
  TSet<FName> Seen;
  for (const auto& C:Choices) { if (C.Value.IsNone() || Seen.Contains(C.Value)) return false; Seen.Add(C.Value); }
 }
 int32 N; return Normalize(DefaultValue,N);
}
void FTASettingsCatalog::RefreshCandidates(FTASettingDefinition& D)
{
 if (D.ChoiceProviderId != TEXT("ScreenResolutions")) return;
 TArray<FIntPoint> Resolutions;
 UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
 FDisplayMetrics Metrics; FDisplayMetrics::RebuildDisplayMetrics(Metrics);
 const FIntPoint Native(Metrics.PrimaryDisplayWidth, Metrics.PrimaryDisplayHeight);
 if (Native.X > 0 && Native.Y > 0) Resolutions.AddUnique(Native);
 if (const auto* U=UGameUserSettings::GetGameUserSettings())
 {
  const auto Current=U->GetScreenResolution();
  if (Current.X>0 && Current.Y>0) Resolutions.AddUnique(Current);
 }
 if (Resolutions.IsEmpty()) Resolutions.Add(FIntPoint(1280,720));
 Resolutions.Sort([](const FIntPoint& A,const FIntPoint& B){ return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
 D.Choices.Reset();
 for (const auto& R : Resolutions)
 {
  FTASettingChoice C; C.Value = FName(*FString::Printf(TEXT("%dx%d"),R.X,R.Y)); D.Choices.Add(C);
 }
 D.DefaultValue=Resolutions.Contains(Native)?Resolutions.IndexOfByKey(Native):Resolutions.Num()-1;
}
TArray<FTASettingDefinition> FTASettingsCatalog::Build(UObject* Outer)
{
 TArray<FTASettingDefinition> Result;
 auto Add = [&](const TCHAR* Id, FName Page, ETASettingType Type, int32 Default)->FTASettingDefinition&
 {
  FTASettingDefinition D; D.SettingId=Id; D.ApplyHandlerId=Id; D.Type=Type;
  FString Section=Page.ToString().Replace(TEXT("Settings.Page."),TEXT("Settings.Section."));
  FString Suffix=Page==TEXT("Settings.Page.Display")?TEXT(".Video"):Page==TEXT("Settings.Page.Game")?TEXT(".General"):TEXT(".Camera");
  const FString Setting(Id);
  if (Setting.EndsWith(TEXT("KeyBindingsMenu"))) Suffix=TEXT(".Bindings");
  if (Setting==TEXT("Controller.MenuCursorSpeed")) Suffix=TEXT(".Cursor");
  if (Setting==TEXT("Display.AdvancedData")) Suffix=TEXT(".Information");
  FTASettingLocation L; L.PageId=Page; L.SectionId=FName(*(Section+Suffix)); D.Locations.Items.Add(L); D.DefaultValue=Default;
  D.NameTextId=FString(TEXT("Settings."))+Id+TEXT(".Name"); D.DescriptionTextId=FString(TEXT("Settings."))+Id+TEXT(".Description");
  Result.Add(D); return Result.Last();
 };
 auto Choices = [](FTASettingDefinition& D, std::initializer_list<const TCHAR*> Values)
 {
  for (const TCHAR* V : Values) { FTASettingChoice C; C.Value=V; C.NameTextId=FString(TEXT("Settings.Value."))+V; D.Choices.Add(C); }
 };
 auto Slider = [&](const TCHAR* Id, FName P, float Default,float Min,float Max,float Step,ETASettingDisplayFormat Format)->FTASettingDefinition&
 {
  auto& D=Add(Id,P,ETASettingType::Slider,1); D.Minimum=Min; D.Maximum=Max; D.PhysicalStep=Step; D.Step=1; D.DefaultValue=D.FromSliderNumber(Default); D.DisplayFormat=Format; return D;
 };
 auto Menu = [&](const TCHAR* Id,FName P,ETASettingSubmenuTarget Kind,const TCHAR* MenuId)
 {
  auto& D=Add(Id,P,ETASettingType::Submenu,0); D.ApplyHandlerId=NAME_None;
  D.TargetMenuDefinition=NewObject<UTASettingsMenuDefinitionAsset>(Outer);
  D.TargetMenuDefinition->MenuId=MenuId; D.TargetMenuDefinition->TitleTextId=D.NameTextId; D.TargetMenuDefinition->MenuKind=Kind;
  if (Kind == ETASettingSubmenuTarget::Brightness) D.TargetMenuDefinition->SettingIds.Add(TEXT("Display.Brightness"));
 };
 auto& Language=Add(TEXT("Game.Language"),FName(TEXT("Settings.Page.Game")),ETASettingType::Choice,0);
 Choices(Language,{TEXT("zh-CN"),TEXT("zh-TW"),TEXT("en")});
 auto& TextSpeed=Slider(TEXT("Game.DialogueTextSpeed"),FName(TEXT("Settings.Page.Game")),40,10,100,5,ETASettingDisplayFormat::Integer);
 TextSpeed.UnitTextId=TEXT("Settings.Unit.CharactersPerSecond"); TextSpeed.ApplyPolicy=ETASettingApplyPolicy::NextDialogue;
 auto& Mode=Add(TEXT("Display.WindowMode"),FName(TEXT("Settings.Page.Display")),ETASettingType::Choice,1);
 Choices(Mode,{TEXT("Fullscreen"),TEXT("WindowedFullscreen"),TEXT("Windowed")}); Mode.ApplyPolicy=ETASettingApplyPolicy::VideoMode;
 auto& Resolution=Add(TEXT("Display.Resolution"),FName(TEXT("Settings.Page.Display")),ETASettingType::Choice,0);
 Resolution.ChoiceProviderId=TEXT("ScreenResolutions"); RefreshCandidates(Resolution); Resolution.ApplyPolicy=ETASettingApplyPolicy::VideoMode;
 Add(TEXT("Display.VSync"),FName(TEXT("Settings.Page.Display")),ETASettingType::Toggle,0);
 auto& Limit=Add(TEXT("Display.FrameRateLimit"),FName(TEXT("Settings.Page.Display")),ETASettingType::Choice,1);
 Choices(Limit,{TEXT("30"),TEXT("60"),TEXT("90"),TEXT("120"),TEXT("144"),TEXT("165"),TEXT("240"),TEXT("Unlimited")});
 Menu(TEXT("Display.BrightnessMenu"),FName(TEXT("Settings.Page.Display")),ETASettingSubmenuTarget::Brightness,TEXT("Menu.Brightness"));
 Slider(TEXT("Display.Brightness"),FName(TEXT("Settings.Page.Display")),2.2f,1,3,0.1f,ETASettingDisplayFormat::Gamma).bSubmenuOnly=true;
 auto& Data=Add(TEXT("Display.AdvancedData"),FName(TEXT("Settings.Page.Display")),ETASettingType::Choice,0);
 Choices(Data,{TEXT("Off"),TEXT("Basic"),TEXT("Full")});
 for (const auto P : {FName(TEXT("Settings.Page.MouseKeyboard")), FName(TEXT("Settings.Page.Controller"))})
 {
  const FString Prefix=P==FName(TEXT("Settings.Page.Controller")) ? TEXT("Controller.") : TEXT("MouseKeyboard.");
  Slider(*(Prefix+TEXT("CameraSensitivity")),P,1,0.1f,3,0.1f,ETASettingDisplayFormat::Multiplier);
  Add(*(Prefix+TEXT("InvertCameraX")),P,ETASettingType::Toggle,0);
  Add(*(Prefix+TEXT("InvertCameraY")),P,ETASettingType::Toggle,0);
  Menu(*(Prefix+TEXT("KeyBindingsMenu")),P,P==FName(TEXT("Settings.Page.Controller"))?ETASettingSubmenuTarget::ControllerKeyBindings:ETASettingSubmenuTarget::KeyBindings,
   P==FName(TEXT("Settings.Page.Controller"))?TEXT("Menu.ControllerKeyBindings"):TEXT("Menu.KeyBindings"));
 }
 Slider(TEXT("Controller.MenuCursorSpeed"),FName(TEXT("Settings.Page.Controller")),1,0.25f,2,0.05f,ETASettingDisplayFormat::Percent);
 return Result;
}
