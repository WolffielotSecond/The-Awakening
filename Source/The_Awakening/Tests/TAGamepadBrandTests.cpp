#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAGamepadBrandResolver.h"
#include "Core/TAInputIconSubsystem.h"
#include "Settings/TASettingsTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "UObject/Package.h"

namespace { template<typename E> int32 I(E Value) { return static_cast<int32>(Value); } }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAGamepadBrandMappingTest,"TheAwakening.Input.GamepadBrand.Mapping",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAGamepadBrandMappingTest::RunTest(const FString&)
{
 using R=FTAGamepadBrandResolver;
 TestEqual(TEXT("Steam PS5 controller"),I(R::FromSteamInputType(13)),I(ETAGamepadBrand::PlayStation));
 TestEqual(TEXT("Steam PS4 controller"),I(R::FromSteamInputType(5)),I(ETAGamepadBrand::PlayStation));
 TestEqual(TEXT("Steam Xbox One controller"),I(R::FromSteamInputType(3)),I(ETAGamepadBrand::Xbox));
 TestEqual(TEXT("Steam Switch Pro uses Xbox prompts"),I(R::FromSteamInputType(10)),I(ETAGamepadBrand::Xbox));
 TestEqual(TEXT("Steam unknown type"),I(R::FromSteamInputType(0)),I(ETAGamepadBrand::Unknown));
 TestEqual(TEXT("Sony pad"),I(R::FromVendorIds({0x054C})),I(ETAGamepadBrand::PlayStation));
 TestEqual(TEXT("Steam virtual pad is ignored"),I(R::FromVendorIds({0x28DE,0x054C})),I(ETAGamepadBrand::PlayStation));
 TestEqual(TEXT("Microsoft pad"),I(R::FromVendorIds({0x045E})),I(ETAGamepadBrand::Xbox));
 TestEqual(TEXT("Nintendo pad uses Xbox prompts"),I(R::FromVendorIds({0x057E})),I(ETAGamepadBrand::Xbox));
 TestEqual(TEXT("Mixed brands are ambiguous"),I(R::FromVendorIds({0x054C,0x045E})),I(ETAGamepadBrand::Unknown));
 TestEqual(TEXT("No pads"),I(R::FromVendorIds({})),I(ETAGamepadBrand::Unknown));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAGamepadBrandDetectionTest,"TheAwakening.Input.GamepadBrand.Detection",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAGamepadBrandDetectionTest::RunTest(const FString&)
{
 auto* Icons=NewObject<UTAInputIconSubsystem>(NewObject<UGameInstance>());
 double Now=10; int32 Calls=0; int32 LastSlot=-2; ETAGamepadBrand Next=ETAGamepadBrand::PlayStation;
 Icons->SetTimeSourceForTests([&Now]{return Now;});
 Icons->SetGamepadBrandProbe([&](int32 Slot){++Calls; LastSlot=Slot; return Next;});
 const FKey Pad=EKeys::Gamepad_FaceButton_Bottom;

 Icons->NotifyInputKey(EKeys::Q);
 TestEqual(TEXT("Keyboard stays keyboard"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::KeyboardMouse));
 Icons->NotifyInputKey(Pad);
 TestEqual(TEXT("Switching to a gamepad checks the pad"),Calls,1);
 TestEqual(TEXT("DualSense detected"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::PS5));
 Icons->NotifyInputKey(Pad);
 TestEqual(TEXT("Known pad is cached"),Calls,1);

 Icons->InvalidateGamepadBrands(); Next=ETAGamepadBrand::Unknown;
 Icons->NotifyInputKey(EKeys::Q); Icons->NotifyInputKey(Pad);
 TestEqual(TEXT("Invalidated cache checks again"),Calls,2);
 TestEqual(TEXT("Unknown pad uses Xbox prompts"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::Xbox));
 Icons->NotifyInputKey(Pad);
 TestEqual(TEXT("Unknown result retries only after the interval"),Calls,2);
 Now+=1; Next=ETAGamepadBrand::PlayStation;
 Icons->NotifyInputKey(Pad);
 TestEqual(TEXT("Retry after interval"),Calls,3);
 TestEqual(TEXT("Late Steam result is applied"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::PS5));

 {
  FInputDeviceScope Scope(nullptr,TEXT("XInputInterface"),2,TEXT("XInputController"));
  Next=ETAGamepadBrand::Xbox;
  Icons->NotifyInputKey(Pad);
 }
 TestEqual(TEXT("Another XInput slot is checked immediately"),Calls,4);
 TestEqual(TEXT("Probe receives the XInput slot"),LastSlot,2);
 TestEqual(TEXT("Second pad is Xbox"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::Xbox));

 Icons->SetGamepadIconStyle(ETAGamepadIconStyle::DualSense);
 TestEqual(TEXT("DualSense style overrides detection"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::PS5));
 Icons->SetGamepadIconStyle(ETAGamepadIconStyle::Xbox);
 TestEqual(TEXT("Xbox style overrides detection"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::Xbox));
 Icons->NotifyInputKey(EKeys::Q);
 Icons->SetGamepadIconStyle(ETAGamepadIconStyle::DualSense);
 TestEqual(TEXT("Style never affects keyboard prompts"),I(Icons->GetCurrentDeviceType()),I(EInputDeviceType::KeyboardMouse));
 TestEqual(TEXT("Gamepad-only query follows the style"),Icons->GetGamepadIconForKey(Pad)?Icons->GetGamepadIconForKey(Pad)->GetName():FString(),FString(TEXT("PlayStation_Button_Cross")));

 Icons->SetCurrentDeviceType(EInputDeviceType::PS5);
 UTexture2D* First=Icons->GetIconForKey(Pad);
 TestTrue(TEXT("Cached icon is reused"),First && First==Icons->GetIconForKey(Pad));
 Icons->SetCurrentDeviceType(EInputDeviceType::Switch);
 UTexture2D* SwitchIcon=Icons->GetIconForKey(Pad);
 TestEqual(TEXT("Unsupported Switch type uses Xbox icons"),SwitchIcon?SwitchIcon->GetName():FString(),FString(TEXT("Xbox_Button_A")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAControllerIconStyleSettingTest,"TheAwakening.Input.GamepadBrand.Setting",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAControllerIconStyleSettingTest::RunTest(const FString&)
{
 const auto Definitions=FTASettingsCatalog::Build(GetTransientPackage());
 const auto* D=Definitions.FindByPredicate([](const FTASettingDefinition& Item){return Item.SettingId==TEXT("Controller.IconStyle");});
 if (!TestNotNull(TEXT("Controller style setting exists"),D)) return false;
 TestEqual(TEXT("Choice setting"),I(D->Type),I(ETASettingType::Choice));
 TestEqual(TEXT("Defaults to auto"),D->GetChoiceValue(D->DefaultValue),FName(TEXT("Auto")));
 TestTrue(TEXT("Three styles"),D->Choices.Num()==3 && D->FindChoiceIndex(TEXT("Xbox"))==1 && D->FindChoiceIndex(TEXT("DualSense"))==2);
 TestTrue(TEXT("Shown with controller bindings"),D->BelongsTo(TEXT("Settings.Page.Controller"),TEXT("Settings.Section.Controller.Bindings")));
 TestTrue(TEXT("Valid definition"),D->IsValidDefinition());
 return true;
}
#endif
