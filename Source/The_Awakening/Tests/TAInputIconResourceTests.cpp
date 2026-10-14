#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Core/TAInputIconSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInputIconResourcesTest,"TheAwakening.Input.IconResources",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAInputIconResourcesTest::RunTest(const FString&)
{
 TArray<FString> CookDirectories;
 GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"),TEXT("DirectoriesToAlwaysCook"),CookDirectories,GGameIni);
 TestTrue(TEXT("Dynamic icon library has an explicit cook root"),CookDirectories.Contains(TEXT("(Path=\"/Game/Textures/Keys\")")));
 auto* Icons=NewObject<UTAInputIconSubsystem>(NewObject<UGameInstance>());
 for (const auto Device:{EInputDeviceType::KeyboardMouse,EInputDeviceType::Xbox,EInputDeviceType::PS5})
 {
  Icons->SetCurrentDeviceType(Device);
  const TArray<FKey> Keys=Device==EInputDeviceType::KeyboardMouse?
   TArray<FKey>{EKeys::Q,EKeys::Tab,EKeys::SpaceBar,EKeys::LeftMouseButton,EKeys::RightMouseButton,EKeys::MouseScrollUp,EKeys::MouseScrollDown,EKeys::MouseWheelAxis}:
   TArray<FKey>{EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Right,EKeys::Gamepad_Left2D,EKeys::Gamepad_Special_Left,EKeys::Gamepad_RightStick_Up,EKeys::Gamepad_RightStick_Down,EKeys::Gamepad_LeftStick_Left,EKeys::Gamepad_LeftStick_Right};
  for (const auto& Key:Keys)
  {
   auto* Texture=Icons->GetIconForKey(Key);
   TestNotNull(FString::Printf(TEXT("Device %d key %s loads through the runtime path"),int32(Device),*Key.ToString()),Texture);
   if (Texture && Key == EKeys::MouseScrollUp) TestEqual(TEXT("Up wheel uses the upward icon"), Texture->GetName(), FString(TEXT("Mouse_Scroll_Up")));
   if (Texture && Key == EKeys::MouseScrollDown) TestEqual(TEXT("Down wheel uses the downward icon"), Texture->GetName(), FString(TEXT("Mouse_Scroll_Down")));
   if (Texture && Key == EKeys::MouseWheelAxis) TestEqual(TEXT("Wheel axis uses the bidirectional icon"), Texture->GetName(), FString(TEXT("Mouse_Scroll_Vertical")));
   // NullRHI does not create GPU platform textures. Validate the imported source
   // dimensions here; cooked GPU/display validation belongs to packaged smoke.
#if WITH_EDITORONLY_DATA
   if (Texture) TestTrue(FString::Printf(TEXT("%s has valid imported pixel data"),*Key.ToString()),
    Texture->Source.IsValid() && Texture->Source.GetSizeX()>0 && Texture->Source.GetSizeY()>0);
#endif
  }
 }
 return true;
}
#endif
