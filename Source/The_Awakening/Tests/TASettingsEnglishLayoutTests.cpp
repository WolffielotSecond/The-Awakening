#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "The_AwakeningPlayerController.h"
#include "Settings/TASettingsMenuWidget.h"
#include "Settings/TASettingsPageWidget.h"
#include "Settings/TASettingRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/Pause/TAPauseMenuWidget.h"
#include "UI/Pause/TAPauseMenuOptionWidget.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Types/PaintArgs.h"
#include "Rendering/DrawElements.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsEnglishLayoutTest,"TheAwakening.Settings.EnglishLayout",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsEnglishLayoutTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=PCClass?World->SpawnActor<AThe_AwakeningPlayerController>(PCClass):nullptr;
 if (!TestNotNull(TEXT("Real player controller"),PC)) return false;
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local; World->AddController(PC);
 for (const TCHAR* Name:{TEXT("WBP_SettingChoice"),TEXT("WBP_SettingSlider"),TEXT("WBP_SettingToggle"),TEXT("WBP_SettingSubmenu")})
 {
  const FString Path=FString::Printf(TEXT("/Game/UI/Setting/%s.%s_C"),Name,Name);
  auto* Class=LoadClass<UTASettingRowWidget>(nullptr,*Path);
  auto* Row=Class?CreateWidget<UTASettingRowWidget>(PC,Class):nullptr;
  if (!TestNotNull(Name,Row)) return false;
  auto* Text=Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Name")));
  auto* Slot=Text?Cast<UHorizontalBoxSlot>(Text->Slot):nullptr;
  auto* Box=Text?Cast<UHorizontalBox>(Text->GetParent()):nullptr;
  if (!TestNotNull(TEXT("Authored name slot"),Slot) || !TestNotNull(TEXT("Authored row"),Box)) return false;
  const auto Font=Text->GetFont(); const auto Size=Slot->GetSize(); const auto Padding=Slot->GetPadding();
  const auto Horizontal=Slot->GetHorizontalAlignment(); const auto Vertical=Slot->GetVerticalAlignment(); const bool Wrapped=Text->GetAutoWrapText();
  auto* Spacer=Row->GetWidgetFromName(TEXT("Spacer_143")); const auto Visibility=Spacer->GetVisibility();
  TMap<UHorizontalBoxSlot*,EVerticalAlignment> Controls;
  for (UWidget* Child:Box->GetAllChildren())
   if (Child!=Text && Child!=Spacer) if (auto* ControlSlot=Cast<UHorizontalBoxSlot>(Child->Slot)) Controls.Add(ControlSlot,ControlSlot->GetVerticalAlignment());
  FTASettingDefinition Definition;
  Definition.Type=FCString::Strstr(Name,TEXT("Slider"))?ETASettingType::Slider:FCString::Strstr(Name,TEXT("Toggle"))?ETASettingType::Toggle:FCString::Strstr(Name,TEXT("Submenu"))?ETASettingType::Submenu:ETASettingType::Choice;
  for (int32 Cycle=0;Cycle<2;++Cycle)
  {
   Row->Configure(Definition,FText::FromString(TEXT("Controller camera sensitivity")),FText::FromString(TEXT("Borderless fullscreen")),75,true,true);
   TestTrue(TEXT("English name wraps in constrained Fill column"),Text->GetAutoWrapText() && Slot->GetSize().SizeRule==ESlateSizeRule::Fill && Slot->GetHorizontalAlignment()==HAlign_Fill);
   TestTrue(TEXT("Explicit minimum gap"),Slot->GetPadding().Right>=24.f);
   TestTrue(TEXT("Spacer no longer competes with English name"),Spacer->GetVisibility()==ESlateVisibility::Collapsed);
   for (const auto& Control:Controls)
   {
    const bool SliderOperations=Definition.Type==ETASettingType::Slider && Control.Key->Content==Row->GetWidgetFromName(TEXT("Overlay_125"));
    TestTrue(TEXT("Slider operations share width; other controls retain Auto"),Control.Key->GetSize().SizeRule==(SliderOperations?ESlateSizeRule::Fill:ESlateSizeRule::Automatic));
    TestTrue(TEXT("Controls stay vertically centered"),Control.Key->GetVerticalAlignment()==VAlign_Center);
   }
   TestTrue(TEXT("Complete font appearance preserved"),Text->GetFont()==Font);
   Row->Configure(Definition,FText::FromString(TEXT("中文")),FText::GetEmpty(),75,true,false);
   if (Definition.Type==ETASettingType::Slider)
   {
    TestTrue(TEXT("Chinese slider uses the same constrained name slot"),Slot->GetSize().SizeRule==ESlateSizeRule::Fill && Text->GetAutoWrapText());
    TestFalse(TEXT("Chinese slider propagates content height"),Cast<USizeBox>(Row->GetRootWidget())->IsHeightOverride());
    continue;
   }
   TestTrue(TEXT("Chinese name slot size restored"),Slot->GetSize().SizeRule==Size.SizeRule && Slot->GetSize().Value==Size.Value);
   TestEqual(TEXT("Chinese padding restored"),Slot->GetPadding(),Padding);
   TestTrue(TEXT("Chinese alignment and wrapping restored"),Slot->GetHorizontalAlignment()==Horizontal && Slot->GetVerticalAlignment()==Vertical && Text->GetAutoWrapText()==Wrapped);
   TestTrue(TEXT("Chinese spacer restored"),Spacer->GetVisibility()==Visibility);
   for (const auto& Control:Controls) TestTrue(TEXT("Chinese control alignment restored"),Control.Key->GetVerticalAlignment()==Control.Value);
  }
 }
 auto* PageClass=LoadClass<UTASettingsPageWidget>(nullptr,TEXT("/Game/UI/Setting/WBP_SettingsPage.WBP_SettingsPage_C"));
 auto* Page=PageClass?CreateWidget<UTASettingsPageWidget>(PC,PageClass):nullptr;
 if (!TestNotNull(TEXT("Real page template"),Page)) return false;
 auto* Label=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("Text_Name")));
 auto* Content=Cast<UButtonSlot>(Label->Slot);
 const auto PageFont=Label->GetFont(); const auto PagePadding=Content->GetPadding(); const auto PageAlignment=Content->GetHorizontalAlignment(); const bool PageWrap=Label->GetAutoWrapText();
 FTASettingsPageDefinition PageDefinition;
 Page->Configure(PageDefinition,FText::FromString(TEXT("Mouse and keyboard")),false,true);
 TestTrue(TEXT("English page wraps and fills button width"),Label->GetAutoWrapText() && Content->GetHorizontalAlignment()==HAlign_Fill);
 TestEqual(TEXT("English page content padding"),Content->GetPadding(),FMargin(12,8));
 auto* Root=Cast<USizeBox>(Page->GetRootWidget());
 TestTrue(TEXT("Page height is content-driven"),Root && !Root->IsHeightOverride());
 TestTrue(TEXT("Page font unchanged"),Label->GetFont()==PageFont);
 Page->Configure(PageDefinition,FText::GetEmpty(),false,false);
 TestTrue(TEXT("Chinese page layout restored"),Content->GetPadding()==PagePadding && Content->GetHorizontalAlignment()==PageAlignment && Label->GetAutoWrapText()==PageWrap);
 auto* MenuClass=LoadClass<UTASettingsMenuWidget>(nullptr,TEXT("/Game/UI/Setting/WBP_SettingsMenu.WBP_SettingsMenu_C"));
 auto* Menu=MenuClass?CreateWidget<UTASettingsMenuWidget>(PC,MenuClass):nullptr;
 if (!TestNotNull(TEXT("Real settings menu template"),Menu)) return false;
 Menu->InitializeResetHoldVisual();
 TMap<UButton*,FMargin> OuterDefaults,InnerDefaults;
 for (UButton* Button:{Menu->Button_RestoreDefaults.Get(),Menu->Button_ConfirmVideoMode.Get(),Menu->Button_RevertVideoMode.Get()})
 {
  OuterDefaults.Add(Button,Cast<UHorizontalBoxSlot>(Button->Slot)->GetPadding());
  InnerDefaults.Add(Button,Cast<UButtonSlot>(Button->GetContent()->Slot)->GetPadding());
 }
 Menu->ApplyLanguageLayout(true);
 for (const auto& Entry:OuterDefaults)
 {
  TestTrue(TEXT("English footer button gap"),Cast<UHorizontalBoxSlot>(Entry.Key->Slot)->GetPadding().Right>=16.f);
  TestEqual(TEXT("English footer content padding"),Cast<UButtonSlot>(Entry.Key->GetContent()->Slot)->GetPadding(),FMargin(12,8));
 }
 Menu->ApplyLanguageLayout(false);
 for (const auto& Entry:OuterDefaults)
 {
  TestEqual(TEXT("Chinese footer gap restored"),Cast<UHorizontalBoxSlot>(Entry.Key->Slot)->GetPadding(),Entry.Value);
  TestEqual(TEXT("Chinese footer padding restored"),Cast<UButtonSlot>(Entry.Key->GetContent()->Slot)->GetPadding(),InnerDefaults.FindChecked(Entry.Key));
 }
 return true;
}

namespace
{
 TOptional<FGeometry> FindLayoutGeometry(const TSharedRef<SWidget>& Widget,const FGeometry& Geometry,const TSharedPtr<SWidget>& Target)
 {
  if (Widget==Target) return Geometry;
  FArrangedChildren Children(EVisibility::All);
  Widget->ArrangeChildren(Geometry,Children);
  for (int32 Index=0;Index<Children.Num();++Index)
  {
   const auto& Child=Children[Index];
   auto Found=FindLayoutGeometry(Child.Widget,Child.Geometry,Target);
   if (Found.IsSet()) return Found;
  }
  return {};
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAEnglishUIBoundsTest,"TheAwakening.Settings.EnglishUIBounds",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAEnglishUIBoundsTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=PCClass?World->SpawnActor<AThe_AwakeningPlayerController>(PCClass):nullptr;
 if (!TestNotNull(TEXT("Player controller"),PC)) return false;
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local; World->AddController(PC);
 auto* RowClass=LoadClass<UTASettingRowWidget>(nullptr,TEXT("/Game/UI/Setting/WBP_SettingSlider.WBP_SettingSlider_C"));
 auto* Row=RowClass?CreateWidget<UTASettingRowWidget>(PC,RowClass):nullptr;
 if (!TestNotNull(TEXT("Real slider row"),Row)) return false;
 auto* Name=Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Name")));
 auto* Value=Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Value")));
 auto* Root=Cast<USizeBox>(Row->GetRootWidget());
 const auto Font=Name->GetFont();
 FTASettingDefinition Definition; Definition.Type=ETASettingType::Slider;
 const auto OriginalValueFont=Value->GetFont();
 auto& Intl=FInternationalization::Get();
 const FString OriginalLanguage=Intl.GetCurrentLanguage()->GetName();
 ON_SCOPE_EXIT { Intl.SetCurrentLanguage(OriginalLanguage); };
 auto Window=SNew(SWindow);
 // Slate Paint supplies AutoWrap's available width; subsequent native prepasses
 // propagate the wrapped desired height. No explicit line breaks or runtime polling.
 for (int32 LanguageCycle=0;LanguageCycle<2;++LanguageCycle)
 for (const TCHAR* Language:{TEXT("en"),TEXT("zh-CN"),TEXT("zh-TW"),TEXT("en")})
 for (float Width:{600.f,741.215f,896.930f,1100.f})
 for (float Scale:{0.6883f,1.3620f})
 for (int32 Number:{10,75,100})
 {
  const bool English=FString(Language)==TEXT("en");
  Intl.SetCurrentLanguage(Language);
  const FString Label=English?TEXT("Dialogue text speed"):FString(Language)==TEXT("zh-CN")?TEXT("对话文字速度"):TEXT("對話文字速度");
  const FString Unit=English?TEXT("characters/s"):FString(Language)==TEXT("zh-CN")?TEXT("字符/秒"):TEXT("字元/秒");
  Row->Configure(Definition,FText::FromString(Label),FText::FromString(FString::Printf(TEXT("%d %s"),Number,*Unit)),Number,true,English);
  auto SlateRow=Row->TakeWidget();
  FGeometry RowGeometry;
  for (int32 Pass=0;Pass<8;++Pass)
  {
   SlateRow->SlatePrepass(Scale);
   RowGeometry=FGeometry::MakeRoot(FVector2D(Width+8.f,SlateRow->GetDesiredSize().Y),FSlateLayoutTransform(Scale));
   FSlateWindowElementList Elements(Window);
   SlateRow->Paint(FPaintArgs(nullptr,Window->GetHittestGrid(),FVector2D::ZeroVector,0,0),RowGeometry,
    FSlateRect(-100000,-100000,100000,100000),Elements,0,FWidgetStyle(),true);
  }
  auto NameGeometry=FindLayoutGeometry(SlateRow,RowGeometry,Name->GetCachedWidget());
  auto ValueGeometry=FindLayoutGeometry(SlateRow,RowGeometry,Value->GetCachedWidget());
  auto ButtonGeometry=FindLayoutGeometry(SlateRow,RowGeometry,Row->GetWidgetFromName(TEXT("Button_Option"))->GetCachedWidget());
  if (!TestTrue(TEXT("Arranged text and button found"),NameGeometry.IsSet() && ValueGeometry.IsSet() && ButtonGeometry.IsSet())) return false;
  TestFalse(TEXT("All languages use content height"),Root->IsHeightOverride());
  TestTrue(TEXT("Label gets a nonzero proportional width"),NameGeometry->GetLocalSize().X>100);
  if (English && Width<900) TestTrue(TEXT("Actual automatic wrapping occurs"),Name->GetDesiredSize().Y>Value->GetFont().Size);
  TestTrue(TEXT("Allocated height contains wrapped label"),NameGeometry->GetLocalSize().Y+1>=Name->GetDesiredSize().Y);
  TestTrue(TEXT("Allocated height contains wrapped value"),ValueGeometry->GetLocalSize().Y+1>=Value->GetDesiredSize().Y);
  TestTrue(TEXT("Wrapped name fits its allocated width"),NameGeometry->GetLocalSize().X+1>=Name->GetDesiredSize().X);
  TestTrue(TEXT("Wrapped value fits its allocated width"),ValueGeometry->GetLocalSize().X+1>=Value->GetDesiredSize().X);
  const double NameBottom=NameGeometry->LocalToAbsolute(FVector2D(0,NameGeometry->GetLocalSize().Y)).Y;
  const double ButtonBottom=ButtonGeometry->LocalToAbsolute(ButtonGeometry->GetLocalSize()).Y;
  TestTrue(TEXT("Label plus padding fits button"),NameBottom+12*Scale<=ButtonBottom+1);
  TestTrue(TEXT("Name and value have distinct horizontal space"),NameGeometry->LocalToAbsolute(FVector2D(NameGeometry->GetLocalSize().X,0)).X+24*Scale<=ValueGeometry->GetAbsolutePosition().X+1);
  const double ButtonCenter=ButtonGeometry->LocalToAbsolute(ButtonGeometry->GetLocalSize()*0.5).Y;
  TOptional<FGeometry> Last;
  for (const TCHAR* ControlName:{TEXT("Text_Value"),TEXT("Button_Decrease"),TEXT("Slider_Value"),TEXT("Button_Increase")})
  {
   auto* Control=Row->GetWidgetFromName(ControlName);
   auto Geometry=FindLayoutGeometry(SlateRow,RowGeometry,Control->GetCachedWidget());
   if (!TestTrue(TEXT("Arranged slider control found"),Geometry.IsSet())) return false;
   TestTrue(TEXT("Controls stay vertically centered"),FMath::IsNearlyEqual(Geometry->LocalToAbsolute(Geometry->GetLocalSize()*0.5).Y,ButtonCenter,1));
   TestTrue(TEXT("Control has positive width"),Geometry->GetLocalSize().X>0);
   TestTrue(TEXT("Every control stays within the button right edge"),Geometry->LocalToAbsolute(FVector2D(Geometry->GetLocalSize().X,0)).X<=ButtonGeometry->LocalToAbsolute(FVector2D(ButtonGeometry->GetLocalSize().X,0)).X+1);
   if (Last.IsSet()) TestTrue(TEXT("Operations do not overlap horizontally"),Last->LocalToAbsolute(FVector2D(Last->GetLocalSize().X,0)).X<=Geometry->GetAbsolutePosition().X+1);
   Last=Geometry;
  }
  TestTrue(TEXT("Both fonts unchanged"),Name->GetFont()==Font && Value->GetFont()==OriginalValueFont);
  if (!English && Width>=1100)
  {
   TestTrue(TEXT("Chinese name stays single line when space is sufficient"),Name->GetDesiredSize().Y<2*Name->GetFont().Size);
   TestTrue(TEXT("Chinese value stays single line when space is sufficient"),Value->GetDesiredSize().Y<2*Value->GetFont().Size);
  }
 }

 auto* MenuClass=LoadClass<UTAPauseMenuWidget>(nullptr,TEXT("/Game/UI/Pause/WBP_PauseMenu.WBP_PauseMenu_C"));
 auto* Menu=MenuClass?CreateWidget<UTAPauseMenuWidget>(PC,MenuClass):nullptr;
 if (!TestNotNull(TEXT("Real pause menu"),Menu)) return false;
 Menu->BuildOptions();
 auto* OptionsSlot=Cast<UCanvasPanelSlot>(Menu->Box_Options->Slot);
 const auto OriginalOffsets=OptionsSlot->GetOffsets();
 const TCHAR* Labels[]={TEXT("Resume Game"),TEXT("Settings"),TEXT("Quit Game")};
 int32 Index=0;
 TMap<UTextBlock*,FSlateFontInfo> Fonts;
 TMap<UTextBlock*,FMargin> Paddings;
 for (const auto& Entry:Menu->OptionWidgetsById)
 {
  auto* Text=Cast<UTextBlock>(Entry.Value->GetWidgetFromName(TEXT("Text_Option")));
  Fonts.Add(Text,Text->GetFont()); Paddings.Add(Text,Cast<UButtonSlot>(Text->Slot)->GetPadding());
  Entry.Value->ConfigureOption(Entry.Key,FText::FromString(Labels[Index++]));
 }
 Menu->ApplyLanguageLayout(true);
 auto SlateOptions=Menu->Box_Options->TakeWidget(); SlateOptions->SlatePrepass();
 const FGeometry OptionsGeometry=FGeometry::MakeRoot(FVector2D(OptionsSlot->GetOffsets().Right,SlateOptions->GetDesiredSize().Y),FSlateLayoutTransform());
 double ButtonWidth=-1;
 for (const auto& Entry:Menu->OptionWidgetsById)
 {
  auto* Text=Cast<UTextBlock>(Entry.Value->GetWidgetFromName(TEXT("Text_Option")));
  auto Geometry=FindLayoutGeometry(SlateOptions,OptionsGeometry,Entry.Value->GetFocusTarget()->GetCachedWidget());
  if (!TestTrue(TEXT("Arranged pause button found"),Geometry.IsSet())) return false;
  const double Width=Geometry->GetLocalSize().X;
  if (ButtonWidth<0) ButtonWidth=Width;
  TestTrue(TEXT("Every pause button has the same width"),FMath::IsNearlyEqual(Width,ButtonWidth,0.1));
  TestTrue(TEXT("Unwrapped label plus side padding fits"),Width+0.1>=Text->GetDesiredSize().X+32);
  TestFalse(TEXT("Pause labels remain single line"),Text->GetAutoWrapText());
  TestTrue(TEXT("Pause font preserved"),Text->GetFont()==Fonts.FindChecked(Text));
 }
 TestTrue(TEXT("Pause menu expands past authored 200-unit width"),OptionsSlot->GetOffsets().Right>OriginalOffsets.Right);
 Menu->ApplyLanguageLayout(false);
 TestEqual(TEXT("Chinese pause container restored"),OptionsSlot->GetOffsets(),OriginalOffsets);
 for (const auto& Entry:Paddings) TestEqual(TEXT("Chinese button padding restored"),Cast<UButtonSlot>(Entry.Key->Slot)->GetPadding(),Entry.Value);
 return true;
}
#endif
