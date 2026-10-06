#include "Settings/TASettingsTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformMisc.h"

bool FTASettingDefinition::Normalize(const FTASettingValue& Input, FTASettingValue& Output) const
{
 if (Type == ETASettingType::Submenu || Input.Kind != DefaultValue.Kind) return false;
 Output = Input;
 if (Type == ETASettingType::Slider)
 {
  if (!FMath::IsFinite(Input.Number) || !FMath::IsFinite(Step) || Step <= 0 || Maximum < Minimum) return false;
  const float N = FMath::Clamp(Input.Number, Minimum, Maximum);
  Output.Number = FMath::Clamp(Minimum + FMath::RoundToFloat((N-Minimum)/Step)*Step, Minimum, Maximum);
 }
 if (Type == ETASettingType::Choice && !Choices.ContainsByPredicate([&](const FTASettingChoice& C) { return C.Value == Input.Choice; })) return false;
 return true;
}
bool FTASettingDefinition::IsValidDefinition() const
{
 if (SettingId.IsNone() || NameTextId.IsEmpty() || DescriptionTextId.IsEmpty()) return false;
 if (Type == ETASettingType::Submenu) return DefaultValue.Kind == ETASettingValueKind::None && TargetMenuDefinition && TargetMenuDefinition->MenuKind != ETASettingSubmenuTarget::None;
 const ETASettingValueKind Expected = Type == ETASettingType::Toggle ? ETASettingValueKind::Boolean :
  Type == ETASettingType::Slider ? ETASettingValueKind::Number : ETASettingValueKind::Choice;
 if (DefaultValue.Kind != Expected) return false;
 TSet<FName> Seen;
 for (const auto& C : Choices) { if (C.Value.IsNone() || Seen.Contains(C.Value)) return false; Seen.Add(C.Value); }
 if (Type == ETASettingType::Slider && (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || !FMath::IsFinite(Step) || Step <= 0 || Maximum < Minimum ||
  DefaultValue.Number < Minimum || DefaultValue.Number > Maximum)) return false;
 FTASettingValue V; return Normalize(DefaultValue, V);
}
void FTASettingsCatalog::RefreshCandidates(FTASettingDefinition& D)
{
 if (D.ChoiceProviderId != TEXT("ScreenResolutions")) return;
 TArray<FIntPoint> Resolutions;
 UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
 FDisplayMetrics Metrics; FDisplayMetrics::RebuildDisplayMetrics(Metrics);
 const FIntPoint Native(Metrics.PrimaryDisplayWidth, Metrics.PrimaryDisplayHeight);
 if (Native.X > 0 && Native.Y > 0) Resolutions.AddUnique(Native);
 if (Resolutions.IsEmpty()) Resolutions.Add(FIntPoint(1280,720));
 Resolutions.Sort([](const FIntPoint& A,const FIntPoint& B){ return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
 D.Choices.Reset();
 for (const auto& R : Resolutions)
 {
  FTASettingChoice C; C.Value = FName(*FString::Printf(TEXT("%dx%d"),R.X,R.Y)); D.Choices.Add(C);
 }
 D.DefaultValue = FTASettingValue::ChoiceValue(FName(*FString::Printf(TEXT("%dx%d"), Resolutions.Last().X, Resolutions.Last().Y)));
 if (Resolutions.Contains(Native)) D.DefaultValue = FTASettingValue::ChoiceValue(FName(*FString::Printf(TEXT("%dx%d"),Native.X,Native.Y)));
}
TArray<FTASettingDefinition> FTASettingsCatalog::Build(UObject* Outer)
{
 TArray<FTASettingDefinition> Result;
 auto Add = [&](const TCHAR* Id, ETAGameSettingsPage Page, ETASettingType Type, FTASettingValue Default)->FTASettingDefinition&
 {
  FTASettingDefinition D; D.SettingId=Id; D.ApplyHandlerId=Id; D.Page=Page; D.Type=Type; D.DefaultValue=Default;
  D.NameTextId=FString(TEXT("Settings."))+Id+TEXT(".Name"); D.DescriptionTextId=FString(TEXT("Settings."))+Id+TEXT(".Description");
  Result.Add(D); return Result.Last();
 };
 auto Choices = [](FTASettingDefinition& D, std::initializer_list<const TCHAR*> Values)
 {
  for (const TCHAR* V : Values) { FTASettingChoice C; C.Value=V; C.NameTextId=FString(TEXT("Settings.Value."))+V; D.Choices.Add(C); }
 };
 auto Slider = [&](const TCHAR* Id, ETAGameSettingsPage P, float Default,float Min,float Max,float Step,ETASettingDisplayFormat Format)->FTASettingDefinition&
 {
  auto& D=Add(Id,P,ETASettingType::Slider,FTASettingValue::Numeric(Default)); D.Minimum=Min; D.Maximum=Max; D.Step=Step; D.DisplayFormat=Format; return D;
 };
 auto Menu = [&](const TCHAR* Id,ETAGameSettingsPage P,ETASettingSubmenuTarget Kind,const TCHAR* MenuId)
 {
  auto& D=Add(Id,P,ETASettingType::Submenu,FTASettingValue()); D.ApplyHandlerId=NAME_None; D.SubmenuTarget=Kind;
  D.TargetMenuDefinition=NewObject<UTASettingsMenuDefinitionAsset>(Outer);
  D.TargetMenuDefinition->MenuId=MenuId; D.TargetMenuDefinition->TitleTextId=D.NameTextId; D.TargetMenuDefinition->MenuKind=Kind;
  if (Kind == ETASettingSubmenuTarget::Brightness) D.TargetMenuDefinition->SettingIds.Add(TEXT("Display.Brightness"));
 };
 auto& Language=Add(TEXT("Game.Language"),ETAGameSettingsPage::Game,ETASettingType::Choice,FTASettingValue::ChoiceValue(TEXT("zh-CN")));
 Choices(Language,{TEXT("zh-CN"),TEXT("zh-TW"),TEXT("en")});
 auto& TextSpeed=Slider(TEXT("Game.DialogueTextSpeed"),ETAGameSettingsPage::Game,40,10,100,5,ETASettingDisplayFormat::Integer);
 TextSpeed.UnitTextId=TEXT("Settings.Unit.CharactersPerSecond"); TextSpeed.ApplyPolicy=ETASettingApplyPolicy::NextDialogue;
 auto& Mode=Add(TEXT("Display.WindowMode"),ETAGameSettingsPage::Display,ETASettingType::Choice,FTASettingValue::ChoiceValue(TEXT("WindowedFullscreen")));
 Choices(Mode,{TEXT("Fullscreen"),TEXT("WindowedFullscreen"),TEXT("Windowed")}); Mode.ApplyPolicy=ETASettingApplyPolicy::VideoMode;
 auto& Resolution=Add(TEXT("Display.Resolution"),ETAGameSettingsPage::Display,ETASettingType::Choice,FTASettingValue::ChoiceValue(TEXT("1280x720")));
 Resolution.ChoiceProviderId=TEXT("ScreenResolutions"); RefreshCandidates(Resolution); Resolution.ApplyPolicy=ETASettingApplyPolicy::VideoMode;
 Add(TEXT("Display.VSync"),ETAGameSettingsPage::Display,ETASettingType::Toggle,FTASettingValue::Boolean(false));
 auto& Limit=Add(TEXT("Display.FrameRateLimit"),ETAGameSettingsPage::Display,ETASettingType::Choice,FTASettingValue::ChoiceValue(TEXT("60")));
 Choices(Limit,{TEXT("30"),TEXT("60"),TEXT("90"),TEXT("120"),TEXT("144"),TEXT("165"),TEXT("240"),TEXT("Unlimited")});
 Menu(TEXT("Display.BrightnessMenu"),ETAGameSettingsPage::Display,ETASettingSubmenuTarget::Brightness,TEXT("Menu.Brightness"));
 Slider(TEXT("Display.Brightness"),ETAGameSettingsPage::Display,2.2f,1,3,0.1f,ETASettingDisplayFormat::Gamma).bSubmenuOnly=true;
 auto& Data=Add(TEXT("Display.AdvancedData"),ETAGameSettingsPage::Display,ETASettingType::Choice,FTASettingValue::ChoiceValue(TEXT("Off")));
 Choices(Data,{TEXT("Off"),TEXT("Basic"),TEXT("Full")});
 for (const auto P : {ETAGameSettingsPage::MouseKeyboard, ETAGameSettingsPage::Controller})
 {
  const FString Prefix=P==ETAGameSettingsPage::Controller ? TEXT("Controller.") : TEXT("MouseKeyboard.");
  Slider(*(Prefix+TEXT("CameraSensitivity")),P,1,0.1f,3,0.1f,ETASettingDisplayFormat::Multiplier);
  Add(*(Prefix+TEXT("InvertCameraX")),P,ETASettingType::Toggle,FTASettingValue::Boolean(false));
  Add(*(Prefix+TEXT("InvertCameraY")),P,ETASettingType::Toggle,FTASettingValue::Boolean(false));
  Menu(*(Prefix+TEXT("KeyBindingsMenu")),P,P==ETAGameSettingsPage::Controller?ETASettingSubmenuTarget::ControllerKeyBindings:ETASettingSubmenuTarget::KeyBindings,
   P==ETAGameSettingsPage::Controller?TEXT("Menu.ControllerKeyBindings"):TEXT("Menu.KeyBindings"));
 }
 Slider(TEXT("Controller.MenuCursorSpeed"),ETAGameSettingsPage::Controller,1,0.25f,2,0.05f,ETASettingDisplayFormat::Percent);
 return Result;
}
