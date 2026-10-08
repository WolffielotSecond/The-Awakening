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
#include "Settings/TASettingsMenuWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsCatalogTest,"TheAwakening.Settings.CatalogValidation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsCatalogTest::RunTest(const FString&)
{
 const auto Definitions=FTASettingsCatalog::Build(GetTransientPackage());
 const auto Pages=FTASettingsCatalog::BuildPages();
 TestEqual(TEXT("Five configurable pages; favorites is not in data"),Pages.Num(),5);
 TSet<FName> PageIds;
 for (const auto& P:Pages)
 {
  TestFalse(TEXT("Unique stable page ID"),P.PageId.IsNone() || PageIds.Contains(P.PageId)); PageIds.Add(P.PageId);
  TestNotEqual(TEXT("Favorites absent from configurable pages"),P.PageId,FTASettingsCatalog::FavoritesPageId());
 }
 FString PageError; TestTrue(TEXT("Valid page definitions"),FTASettingsCatalog::ValidatePages(Pages,PageError));
 for (const auto& D:Definitions) TestTrue(TEXT("Every setting location resolves to a real page and section"),FTASettingsCatalog::ValidateLocations(D,Pages));
 TSet<FName> Seen;
 for(const auto& D:Definitions)
 {
  TestTrue(*D.SettingId.ToString(),D.IsValidDefinition());
  TestFalse(TEXT("Stable setting IDs are unique"),Seen.Contains(D.SettingId)); Seen.Add(D.SettingId);
 }
 TestTrue(TEXT("Controller binding submenu exists"),Seen.Contains(TEXT("Controller.KeyBindingsMenu")));
 TestTrue(TEXT("Advanced data exists"),Seen.Contains(TEXT("Display.AdvancedData")));
 TestFalse(TEXT("Excluded FOV absent"),Seen.Contains(TEXT("Display.FOV")));
 TestFalse(TEXT("No unconnected audio values"),Definitions.ContainsByPredicate([](const auto& D){return D.BelongsTo(TEXT("Settings.Page.Audio"));}));
 for(const TCHAR* Language:{TEXT("zh-CN"),TEXT("zh-TW"),TEXT("en")})
 {
  FString Json; TestTrue(TEXT("Localization file"),FFileHelper::LoadFileToString(Json,*(FPaths::ProjectContentDir()/TEXT("Localization")/(FString(Language)+TEXT(".json")))));
  TSharedPtr<FJsonObject> Object; TestTrue(TEXT("Valid localization JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object));
  if (!Object) continue;
  for (const auto& P:Pages) { TestTrue(*P.PageId.ToString(),Object->HasField(P.PageId.ToString())); for (const auto& S:P.Sections) TestTrue(*S.SectionId.ToString(),Object->HasField(S.SectionId.ToString())); }
  for(const auto& D:Definitions)
  {
   TestTrue(*D.NameTextId,Object->HasField(D.NameTextId)); TestTrue(*D.DescriptionTextId,Object->HasField(D.DescriptionTextId));
   for(const auto& C:D.Choices) if (!C.NameTextId.IsEmpty()) TestTrue(*C.NameTextId,Object->HasField(C.NameTextId));
  }
 }
 auto OrderedPages=Pages; OrderedPages[0].SortOrder=90; OrderedPages[1].SortOrder=-10;
 FTASettingsCatalog::SortPages(OrderedPages);
 TestEqual(TEXT("Editable page order is respected"),OrderedPages[0].PageId,FName(TEXT("Settings.Page.Display")));
 TestEqual(TEXT("High order moves page to the end"),OrderedPages.Last().PageId,FName(TEXT("Settings.Page.Game")));
 auto EqualOrder=Pages; for (auto& P:EqualOrder) P.SortOrder=0;
 FTASettingsCatalog::SortPages(EqualOrder);
 TestEqual(TEXT("Equal order preserves authored array order"),EqualOrder[0].PageId,Pages[0].PageId);
 auto Navigation=FTASettingsCatalog::BuildNavigationPages(OrderedPages);
 TestEqual(TEXT("Favorites fixed first regardless of custom order"),Navigation[0].PageId,FTASettingsCatalog::FavoritesPageId());
 TestEqual(TEXT("Navigation adds one built-in page"),Navigation.Num(),Pages.Num()+1);
 FTASettingsPageDefinition CustomPage; CustomPage.PageId=TEXT("Settings.Page.Accessibility"); CustomPage.SortOrder=-999;
 FTASettingsSectionDefinition CustomSection; CustomSection.SectionId=TEXT("Settings.Section.Accessibility.General"); CustomPage.Sections.Add(CustomSection);
 auto ExpandedPages=Pages; ExpandedPages.Add(CustomPage);
 TestTrue(TEXT("New arbitrary page accepted without enum changes"),FTASettingsCatalog::ValidatePages(ExpandedPages,PageError));
 auto SharedDefinitions=Definitions;
 auto* Shared=SharedDefinitions.FindByPredicate([](const auto& D){return D.SettingId==TEXT("Game.Language");});
 if (!Shared) return false;
 FTASettingLocation Extra; Extra.PageId=CustomPage.PageId; Extra.SectionId=CustomSection.SectionId; Shared->Locations.Items.Add(Extra);
 TestTrue(TEXT("One setting can belong to two pages"),FTASettingsCatalog::ValidateLocations(*Shared,ExpandedPages));
 const auto MainGroups=FTASettingsCatalog::BuildViewGroups(ExpandedPages,SharedDefinitions,CustomPage.PageId,{});
 TestEqual(TEXT("New page has its configured section"),MainGroups.Num(),1);
 if (!MainGroups.IsEmpty()) TestTrue(TEXT("Shared setting displayed in new page"),MainGroups[0].SettingIds.Contains(TEXT("Game.Language")));
 const TSet<FName> Favorites={FName(TEXT("Game.Language"))};
 const auto FavoriteGroups=FTASettingsCatalog::BuildViewGroups(ExpandedPages,SharedDefinitions,FTASettingsCatalog::FavoritesPageId(),Favorites);
 TestEqual(TEXT("Favorite appears under each owning page"),FavoriteGroups.Num(),2);
 if (FavoriteGroups.Num()==2)
 {
  TestEqual(TEXT("Favorite headings are page IDs, not section IDs"),FavoriteGroups[0].HeadingId,CustomPage.PageId);
  TestEqual(TEXT("Second owning page heading"),FavoriteGroups[1].HeadingId,FName(TEXT("Settings.Page.Game")));
 }
 FTASettingsPageDefinition Reserved; Reserved.PageId=FTASettingsCatalog::FavoritesPageId(); ExpandedPages.Add(Reserved);
 TestFalse(TEXT("Favorites cannot be defined in the DA"),FTASettingsCatalog::ValidatePages(ExpandedPages,PageError));
 auto Invalid=*Shared; Invalid.Locations.Items[0].SectionId=TEXT("Removed.Section");
 TestFalse(TEXT("Orphaned section reference rejected"),FTASettingsCatalog::ValidateLocations(Invalid,Pages));
 FTASettingDefinition Slider; Slider.SettingId=TEXT("Test.Slider"); Slider.Type=ETASettingType::Slider;
 Slider.NameTextId=TEXT("Test.Name"); Slider.DescriptionTextId=TEXT("Test.Description");
 Slider.Minimum=0.25f; Slider.Maximum=2; Slider.PhysicalStep=0.05f; Slider.Step=1; Slider.DefaultValue=44;
 int32 Out=0;
 TestTrue(TEXT("Integer slider accepts position"),Slider.Normalize(44,Out));
 TestEqual(TEXT("Storage remains an integer position"),Out,44);
 TestTrue(TEXT("Low position is clamped"),Slider.Normalize(-50,Out)); TestEqual(TEXT("Lower bound is 1"),Out,1);
 TestTrue(TEXT("High position is clamped"),Slider.Normalize(200,Out)); TestEqual(TEXT("Upper bound is 100"),Out,100);
 TestTrue(TEXT("Physical lower endpoint"),FMath::IsNearlyEqual(Slider.ToSliderNumber(1),0.25f));
 TestTrue(TEXT("Physical upper endpoint"),FMath::IsNearlyEqual(Slider.ToSliderNumber(100),2.f));
 TestTrue(TEXT("Decoded cursor default remains one"),FMath::IsNearlyEqual(Slider.ToSliderNumber(44),1.f));
 TestEqual(TEXT("Physical value maps back to nearest integer position"),Slider.FromSliderNumber(1),43);
 TestTrue(TEXT("Valid integer slider definition"),Slider.IsValidDefinition());
 Slider.Step=0; TestFalse(TEXT("Reject zero integer step"),Slider.IsValidDefinition());
 const auto* Advanced=Definitions.FindByPredicate([](const auto& D){return D.SettingId==TEXT("Display.AdvancedData");});
 if (Advanced)
 {
  TestEqual(TEXT("Three advanced data modes"),Advanced->Choices.Num(),3);
  TestTrue(TEXT("Choice accepts array index"),Advanced->Normalize(2,Out));
  TestEqual(TEXT("Choice resolves by array index"),Advanced->GetChoiceValue(2),FName(TEXT("Full")));
  TestFalse(TEXT("Reject negative choice index"),Advanced->Normalize(-1,Out));
  TestFalse(TEXT("Reject out of range choice index"),Advanced->Normalize(3,Out));
 }
 const auto* Toggle=Definitions.FindByPredicate([](const auto& D){return D.SettingId==TEXT("MouseKeyboard.InvertCameraX");});
 if (Toggle) { TestTrue(TEXT("Toggle accepts 1"),Toggle->Normalize(1,Out)); TestFalse(TEXT("Toggle rejects 2"),Toggle->Normalize(2,Out)); }
 const auto* Brightness=Definitions.FindByPredicate([](const auto& D){return D.SettingId==TEXT("Display.BrightnessMenu");});
 if (Brightness)
 {
  auto Direct=*Brightness; Direct.TargetMenuDefinition=nullptr; Direct.TargetMenuWidgetClass=UTABrightnessMenuWidget::StaticClass();
  TestTrue(TEXT("Submenu can reference its WBP class directly"),Direct.IsValidDefinition());
  TestFalse(TEXT("Submenu has no adjustable saved value"),Direct.Normalize(0,Out));
 }

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
 TestTrue(TEXT("Save mouse sensitivity"),State->SetValue(TEXT("MouseKeyboard.CameraSensitivity"),66));
 TestTrue(TEXT("Save controller sensitivity"),State->SetValue(TEXT("Controller.CameraSensitivity"),15));
 State->SetFavorite(TEXT("Controller.KeyBindingsMenu"),true);
 State->SetFavorite(TEXT("Display.AdvancedData"),true);
 State->SetFavorite(TEXT("Removed.Unknown"),true);
 TestEqual(TEXT("Unknown favorite cannot enter save"),State->GetFavoriteSettingIds().Num(),2);
 // Reordering the catalog must preserve both values and ID-based favorites.
 auto* Asset=NewObject<UTASettingsDefinitionAsset>(); Asset->Definitions=State->Definitions;
 Algo::Reverse(Asset->Definitions);
 TestTrue(TEXT("Accept valid reordered catalog"),State->UseDefinitionAsset(Asset));
 TestEqual(TEXT("Catalog change preserves integer value"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),66);
 auto* Reloaded=NewObject<UTASettingsSubsystem>(Player);
 Reloaded->Definitions=Asset->Definitions; Reloaded->LoadSavedValues();
 TestEqual(TEXT("Mouse integer survives save/load"),Reloaded->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),66);
 TestTrue(TEXT("Mouse physical value remains two"),FMath::IsNearlyEqual(Reloaded->GetNumber(TEXT("MouseKeyboard.CameraSensitivity")),2.f));
 TestEqual(TEXT("Controller integer remains independent"),Reloaded->GetInteger(TEXT("Controller.CameraSensitivity")),15);
 TestTrue(TEXT("Controller physical value remains half"),FMath::IsNearlyEqual(Reloaded->GetNumber(TEXT("Controller.CameraSensitivity")),0.5f));
 TestTrue(TEXT("Favorite identity survives order changes"),Reloaded->IsFavorite(TEXT("Controller.KeyBindingsMenu")));
 TestTrue(TEXT("Advanced display favorite survives"),Reloaded->IsFavorite(TEXT("Display.AdvancedData")));
 Reloaded->RestoreSettingDefault(TEXT("Controller.CameraSensitivity"));
 TestTrue(TEXT("Restore controller only"),FMath::IsNearlyEqual(Reloaded->GetNumber(TEXT("Controller.CameraSensitivity")),1.f));
 TestEqual(TEXT("Mouse integer unaffected by controller restore"),Reloaded->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),66);
 TestTrue(TEXT("Store advanced data by index"),Reloaded->SetValue(TEXT("Display.AdvancedData"),2));
 TestTrue(TEXT("Store toggle as one"),Reloaded->SetValue(TEXT("Controller.InvertCameraX"),1));
 FString Encoded;
 GConfig->GetString(TEXT("TheAwakening.PlayerSettings"),TEXT("MouseKeyboard.CameraSensitivity"),Encoded,Filename);
 TestEqual(TEXT("Slider is serialized as a raw integer"),Encoded,FString(TEXT("66")));
 GConfig->GetString(TEXT("TheAwakening.PlayerSettings"),TEXT("Display.AdvancedData"),Encoded,Filename);
 TestEqual(TEXT("Choice is serialized as a raw integer"),Encoded,FString(TEXT("2")));
 int32 Schema=0; GConfig->GetInt(TEXT("TheAwakening.PlayerSettings"),TEXT("SchemaVersion"),Schema,Filename);
 TestEqual(TEXT("Integer storage schema"),Schema,2);
 // Migrate the previously shipped physical slider and semantic choice saves.
 GConfig->SetInt(TEXT("TheAwakening.PlayerSettings"),TEXT("SchemaVersion"),1,Filename);
 GConfig->SetString(TEXT("TheAwakening.PlayerSettings"),TEXT("Controller.CameraSensitivity"),TEXT("0.5"),Filename);
 GConfig->SetString(TEXT("TheAwakening.PlayerSettings"),TEXT("Display.AdvancedData"),TEXT("Full"),Filename);
 auto* Migrated=NewObject<UTASettingsSubsystem>(Player); Migrated->Definitions=State->Definitions; Migrated->LoadSavedValues();
 TestEqual(TEXT("Legacy slider converts to position"),Migrated->GetInteger(TEXT("Controller.CameraSensitivity")),15);
 TestEqual(TEXT("Legacy choice converts to array index"),Migrated->GetInteger(TEXT("Display.AdvancedData")),2);
 Migrated->SaveValues();
 GConfig->GetString(TEXT("TheAwakening.PlayerSettings"),TEXT("Controller.CameraSensitivity"),Encoded,Filename);
 TestEqual(TEXT("Migrated slider persists as integer"),Encoded,FString(TEXT("15")));
 const FTASettingDefinition Duplicate=Asset->Definitions.Last(); Asset->Definitions.Add(Duplicate);
 TestFalse(TEXT("Duplicate stable IDs reject catalog"),Reloaded->UseDefinitionAsset(Asset));
 Reloaded->SetFavorite(TEXT("Controller.KeyBindingsMenu"),false);
 TestFalse(TEXT("Unfavorite removes exact ID"),Reloaded->IsFavorite(TEXT("Controller.KeyBindingsMenu")));
 return true;
}
#endif
