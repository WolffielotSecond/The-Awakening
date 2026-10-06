#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Settings/TASettingsTypes.h"
#include "Settings/TASettingsSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Algo/Reverse.h"
#include "Settings/TAKeyBindingService.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedPlayerInput.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsCatalogTest,"TheAwakening.Settings.CatalogValidation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsCatalogTest::RunTest(const FString&)
{
 const auto Definitions=FTASettingsCatalog::Build(GetTransientPackage());
 TSet<FName> Seen;
 for(const auto& D:Definitions)
 {
  TestTrue(*D.SettingId.ToString(),D.IsValidDefinition());
  TestFalse(TEXT("Stable setting IDs are unique"),Seen.Contains(D.SettingId)); Seen.Add(D.SettingId);
 }
 TestTrue(TEXT("Controller binding submenu exists"),Seen.Contains(TEXT("Controller.KeyBindingsMenu")));
 TestTrue(TEXT("Advanced data exists"),Seen.Contains(TEXT("Display.AdvancedData")));
 TestFalse(TEXT("Excluded FOV absent"),Seen.Contains(TEXT("Display.FOV")));
 TestFalse(TEXT("No unconnected audio values"),Definitions.ContainsByPredicate([](const auto& D){return D.Page==ETAGameSettingsPage::Audio;}));
 for(const TCHAR* Language:{TEXT("zh-CN"),TEXT("zh-TW"),TEXT("en")})
 {
  FString Json; TestTrue(TEXT("Localization file"),FFileHelper::LoadFileToString(Json,*(FPaths::ProjectContentDir()/TEXT("Localization")/(FString(Language)+TEXT(".json")))));
  TSharedPtr<FJsonObject> Object; TestTrue(TEXT("Valid localization JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object));
  if (!Object) continue;
  for(const auto& D:Definitions)
  {
   TestTrue(*D.NameTextId,Object->HasField(D.NameTextId)); TestTrue(*D.DescriptionTextId,Object->HasField(D.DescriptionTextId));
   for(const auto& C:D.Choices) if (!C.NameTextId.IsEmpty()) TestTrue(*C.NameTextId,Object->HasField(C.NameTextId));
  }
 }
 FTASettingDefinition Slider; Slider.SettingId=TEXT("Test.Slider"); Slider.Type=ETASettingType::Slider;
 Slider.Minimum=0.25f; Slider.Maximum=2; Slider.Step=0.1f; Slider.DefaultValue=FTASettingValue::Numeric(1.25f);
 FTASettingValue Out;
 TestTrue(TEXT("Valid number"),Slider.Normalize(FTASettingValue::Numeric(0.31f),Out));
 TestTrue(TEXT("Step is relative to minimum"),FMath::IsNearlyEqual(Out.Number,0.35f));
 TestTrue(TEXT("Clamped value"),Slider.Normalize(FTASettingValue::Numeric(20),Out)); TestEqual(TEXT("Upper bound"),Out.Number,2.f);
 TestFalse(TEXT("Reject wrong type"),Slider.Normalize(FTASettingValue::Boolean(true),Out));
 TestFalse(TEXT("Reject NaN"),Slider.Normalize(FTASettingValue::Numeric(std::numeric_limits<float>::quiet_NaN()),Out));
 Slider.Step=0; TestFalse(TEXT("Reject zero step"),Slider.Normalize(FTASettingValue::Numeric(1),Out));
 const auto* Advanced=Definitions.FindByPredicate([](const auto& D){return D.SettingId==TEXT("Display.AdvancedData");});
 if (Advanced) { TestEqual(TEXT("Three advanced data modes"),Advanced->Choices.Num(),3); TestFalse(TEXT("Reject unknown choice"),Advanced->Normalize(FTASettingValue::ChoiceValue(TEXT("Invalid")),Out)); }
 return true;
}
namespace
{
 struct FSettingsTestInput : IEnhancedInputSubsystemInterface
 {
  UEnhancedPlayerInput* Input=nullptr;
  UEnhancedInputUserSettings* User=nullptr;
  TMap<TObjectPtr<const UInputAction>,FInjectedInput> Injected;
  virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
  virtual UEnhancedInputUserSettings* GetUserSettings() const override { return User; }
  virtual TMap<TObjectPtr<const UInputAction>,FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsBindingTest,"TheAwakening.Settings.BindingRoundTrip",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsBindingTest::RunTest(const FString&)
{
 auto* Owner=NewObject<UInputMappingContext>();
 auto* Source=NewObject<UInputMappingContext>(Owner,TEXT("IMC_SettingsTest"));
 auto* Action=NewObject<UInputAction>(Owner,TEXT("IA_Interact"));
 Source->MapKey(Action,EKeys::E);
 Source->MapKey(Action,EKeys::Gamepad_FaceButton_Bottom);
 auto* Axis=NewObject<UInputAction>(Owner,TEXT("IA_Look")); Axis->ValueType=EInputActionValueType::Axis2D;
 Source->MapKey(Axis,EKeys::Gamepad_Right2D);
 auto* Modifier=NewObject<UInputModifierNegate>(Source); Source->GetMapping(0).Modifiers.Add(Modifier);
 TArray<FTAKeyBindingDefinition> Bindings;
 auto* Runtime=UTAKeyBindingService::BuildRuntimeContext(Source,NewObject<UInputMappingContext>(),Bindings);
 TestTrue(TEXT("Original asset is not modified"),Source->GetMapping(0).GetPlayerMappableKeySettings()==nullptr);
 TestEqual(TEXT("Only buttons are exposed"),Bindings.Num(),2);
 TestTrue(TEXT("Separate device identities"),Bindings.Num()==2 && Bindings[0].BindingId!=Bindings[1].BindingId);
 TestEqual(TEXT("Modifiers retained"),Runtime->GetMapping(0).Modifiers.Num(),1);
 TestTrue(TEXT("Modifier class retained"),Runtime->GetMapping(0).Modifiers[0]->IsA<UInputModifierNegate>());
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* Controller=World->SpawnActor<APlayerController>();
 auto* Player=NewObject<ULocalPlayer>(GEngine); Player->PlayerController=Controller; Controller->Player=Player;
 auto* User=NewObject<UEnhancedInputUserSettings>(); User->Initialize(Player);
 TestTrue(TEXT("Register runtime mappings"),User->RegisterInputMappingContext(Runtime));
 if (Bindings.Num()!=2 || !User->GetActiveKeyProfile()) return false;
 FMapPlayerKeyArgs Args; Args.MappingName=Bindings[1].MappingName; Args.NewKey=EKeys::Gamepad_FaceButton_Right;
 const auto* Row=User->GetActiveKeyProfile()->FindKeyMappingRow(Args.MappingName);
 if (!TestNotNull(TEXT("Controller mapping row"),Row)) return false;
 for(const auto& M:Row->Mappings) { Args.Slot=M.GetSlot(); Args.HardwareDeviceId=M.GetHardwareDeviceId().HardwareDeviceIdentifier; break; }
 FGameplayTagContainer Errors; User->MapPlayerKey(Args,Errors); TestTrue(TEXT("Controller rebind succeeds"),Errors.IsEmpty());
 FSettingsTestInput Resolved; Resolved.Input=NewObject<UEnhancedPlayerInput>(Controller); Resolved.User=User;
 Resolved.AddMappingContext(Runtime,0); FModifyContextOptions Options; Options.bForceImmediately=true; Resolved.RequestRebuildControlMappings(Options);
 TArray<FKey> Keys=Resolved.QueryKeysMappedToAction(Action);
 TestTrue(TEXT("New controller button is resolved"),Keys.Contains(EKeys::Gamepad_FaceButton_Right));
 TestFalse(TEXT("Old controller button removed"),Keys.Contains(EKeys::Gamepad_FaceButton_Bottom));
 TestTrue(TEXT("Keyboard binding unchanged"),Keys.Contains(EKeys::E));
 TArray<uint8> Bytes; TestTrue(TEXT("Serialize user mappings"),UGameplayStatics::SaveGameToMemory(User,Bytes));
 auto* Reloaded=Cast<UEnhancedInputUserSettings>(UGameplayStatics::LoadGameFromMemory(Bytes));
 if (!TestNotNull(TEXT("Reload user mappings"),Reloaded)) return false;
 Reloaded->Initialize(Player); Reloaded->RegisterInputMappingContext(Runtime);
 Resolved.User=Reloaded; Resolved.RequestRebuildControlMappings(Options); Keys=Resolved.QueryKeysMappedToAction(Action);
 TestTrue(TEXT("Controller rebind survives serialization"),Keys.Contains(EKeys::Gamepad_FaceButton_Right));
 Reloaded->GetActiveKeyProfile()->ResetMappingToDefault(Bindings[1].MappingName); Resolved.RequestRebuildControlMappings(Options);
 Keys=Resolved.QueryKeysMappedToAction(Action);
 TestTrue(TEXT("Controller reset restores default"),Keys.Contains(EKeys::Gamepad_FaceButton_Bottom));
 TestTrue(TEXT("Controller reset leaves keyboard"),Keys.Contains(EKeys::E));
 TestTrue(TEXT("Shared menu actions overlap dialogue"),UTAKeyBindingService::GroupsOverlap(TEXT("Menu"),TEXT("Dialogue")));
 TestFalse(TEXT("Gameplay and dialogue may share keys"),UTAKeyBindingService::GroupsOverlap(TEXT("Gameplay"),TEXT("Dialogue")));
 TestFalse(TEXT("Inventory and puzzle are exclusive"),UTAKeyBindingService::GroupsOverlap(TEXT("Inventory"),TEXT("Puzzle")));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsStateTest,"TheAwakening.Settings.StatePersistence",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsStateTest::RunTest(const FString&)
{
 const FString Filename=FPaths::ProjectSavedDir()/TEXT("Automation/SettingsState-Test.ini");
 FFileHelper::SaveStringToFile(TEXT(""),*Filename);
 GConfig->LoadFile(Filename);
 TGuardValue<FString> SettingsFile(GGameUserSettingsIni,Filename);
 ON_SCOPE_EXIT { GConfig->UnloadFile(Filename); IFileManager::Get().Delete(*Filename); };
 auto* Player=NewObject<ULocalPlayer>(GEngine);
 auto* State=NewObject<UTASettingsSubsystem>(Player);
 State->Definitions=FTASettingsCatalog::Build(State);
 TestTrue(TEXT("Save mouse sensitivity"),State->SetValue(TEXT("MouseKeyboard.CameraSensitivity"),FTASettingValue::Numeric(2)));
 TestTrue(TEXT("Save controller sensitivity"),State->SetValue(TEXT("Controller.CameraSensitivity"),FTASettingValue::Numeric(0.5f)));
 State->SetFavorite(TEXT("Controller.KeyBindingsMenu"),true);
 State->SetFavorite(TEXT("Display.AdvancedData"),true);
 State->SetFavorite(TEXT("Removed.Unknown"),true);
 TestEqual(TEXT("Unknown favorite cannot enter save"),State->GetFavoriteSettingIds().Num(),2);
 // Reordering the catalog must preserve both values and ID-based favorites.
 auto* Asset=NewObject<UTASettingsDefinitionAsset>(); Asset->Definitions=State->Definitions;
 Algo::Reverse(Asset->Definitions);
 TestTrue(TEXT("Accept valid reordered catalog"),State->UseDefinitionAsset(Asset));
 TestEqual(TEXT("Opening/catalog change preserves live value"),State->GetNumber(TEXT("MouseKeyboard.CameraSensitivity")),2.f);
 auto* Reloaded=NewObject<UTASettingsSubsystem>(Player);
 Reloaded->Definitions=Asset->Definitions; Reloaded->LoadSavedValues();
 TestEqual(TEXT("Mouse value survives save/load"),Reloaded->GetNumber(TEXT("MouseKeyboard.CameraSensitivity")),2.f);
 TestEqual(TEXT("Controller value remains independent"),Reloaded->GetNumber(TEXT("Controller.CameraSensitivity")),0.5f);
 TestTrue(TEXT("Favorite identity survives order changes"),Reloaded->IsFavorite(TEXT("Controller.KeyBindingsMenu")));
 TestTrue(TEXT("Advanced display favorite survives"),Reloaded->IsFavorite(TEXT("Display.AdvancedData")));
 Reloaded->RestoreSettingDefault(TEXT("Controller.CameraSensitivity"));
 TestEqual(TEXT("Restore controller only"),Reloaded->GetNumber(TEXT("Controller.CameraSensitivity")),1.f);
 TestEqual(TEXT("Mouse unaffected by controller restore"),Reloaded->GetNumber(TEXT("MouseKeyboard.CameraSensitivity")),2.f);
 const FTASettingDefinition Duplicate=Asset->Definitions.Last(); Asset->Definitions.Add(Duplicate);
 TestFalse(TEXT("Duplicate stable IDs reject catalog"),Reloaded->UseDefinitionAsset(Asset));
 Reloaded->SetFavorite(TEXT("Controller.KeyBindingsMenu"),false);
 TestFalse(TEXT("Unfavorite removes exact ID"),Reloaded->IsFavorite(TEXT("Controller.KeyBindingsMenu")));
 return true;
}
#endif
