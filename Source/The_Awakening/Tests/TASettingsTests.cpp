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
#include "Settings/TASettingRowWidget.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TAPlayerInput.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsMenuInputTest,"TheAwakening.Settings.MenuInput",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsMenuInputTest::RunTest(const FString&)
{
 const FString Filename=FPaths::ProjectSavedDir()/TEXT("Automation/SettingsMenuInput-Test.ini");
 FFileHelper::SaveStringToFile(TEXT(""),*Filename); GConfig->LoadFile(Filename);
 TGuardValue<FString> SettingsFile(GGameUserSettingsIni,Filename);
 ON_SCOPE_EXIT { GConfig->UnloadFile(Filename); IFileManager::Get().Delete(*Filename); };
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=PCClass?World->SpawnActor<AThe_AwakeningPlayerController>(PCClass):nullptr;
 if (!TestNotNull(TEXT("Real player controller"),PC)) return false;
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local; World->AddController(PC);
 auto* Class=LoadClass<UTASettingsMenuWidget>(nullptr,TEXT("/Game/UI/Setting/WBP_SettingsMenu.WBP_SettingsMenu_C"));
 auto* Menu=Class?CreateWidget<UTASettingsMenuWidget>(PC,Class):nullptr;
 if (!TestNotNull(TEXT("Real settings menu template"),Menu)) return false;
 Menu->InitializeMenu(PC,nullptr);
 Menu->InitializeResetHoldVisual();
 TestNotNull(TEXT("Existing reset button gains a hold progress overlay"),Menu->ProgressBar_ResetHold.Get());
 if (Menu->ProgressBar_ResetHold)
 {
  if (Menu->Button_RestoreDefaults && Menu->bGeneratedResetHoldVisual)
  {
   TestEqual(TEXT("Reset button has no normal style inset"),Menu->Button_RestoreDefaults->GetStyle().NormalPadding,FMargin(0));
   TestEqual(TEXT("Reset button has no pressed style inset"),Menu->Button_RestoreDefaults->GetStyle().PressedPadding,FMargin(0));
  }
  auto* Overlay=Cast<UOverlay>(Menu->ProgressBar_ResetHold->GetParent());
  TestNotNull(TEXT("Progress is layered inside the reset button"),Overlay);
  UWidget* Label=Menu->Text_RestoreDefaults.Get();
  while (Label && Label->GetParent()!=Overlay) Label=Label->GetParent();
  if (Overlay && TestNotNull(TEXT("Original reset label is preserved"),Label))
   TestTrue(TEXT("Reset text renders above fill"),Overlay->GetChildIndex(Label)>Overlay->GetChildIndex(Menu->ProgressBar_ResetHold));
 }
 auto* AuthoredProgress=Menu->WidgetTree->ConstructWidget<UProgressBar>();
 auto AuthoredStyle=AuthoredProgress->GetWidgetStyle();
 AuthoredStyle.BackgroundImage=FSlateColorBrush(FLinearColor::Red);
 AuthoredProgress->SetWidgetStyle(AuthoredStyle);
 AuthoredProgress->SetFillColorAndOpacity(FLinearColor::Green);
 AuthoredProgress->SetBarFillStyle(EProgressBarFillStyle::Mask);
 AuthoredProgress->SetBorderPadding(FVector2D(7,9));
 Menu->ProgressBar_ResetHold=AuthoredProgress; Menu->bGeneratedResetHoldVisual=false;
 Menu->InitializeResetHoldVisual();
 TestTrue(TEXT("Authored progress instance is used"),Menu->ProgressBar_ResetHold==AuthoredProgress);
 TestEqual(TEXT("Authored fill color survives initialization"),AuthoredProgress->GetFillColorAndOpacity(),FLinearColor::Green);
 TestTrue(TEXT("Authored background survives initialization"),AuthoredProgress->GetWidgetStyle().BackgroundImage==AuthoredStyle.BackgroundImage);
 TestEqual(TEXT("Authored border padding survives initialization"),AuthoredProgress->GetBorderPadding(),FVector2D(7,9));
 TestTrue(TEXT("Authored fill style survives initialization"),AuthoredProgress->GetBarFillStyle()==EProgressBarFillStyle::Mask);
 auto* State=NewObject<UTASettingsSubsystem>(Local); State->Definitions=FTASettingsCatalog::Build(State);
 Menu->SettingsSubsystem=State;
 TestTrue(TEXT("Scroll up remains hidden from prompts"),Menu->GetSettingCommandTextId(TEXT("ScrollUp")).IsEmpty());
 TestTrue(TEXT("Scroll down remains hidden from prompts"),Menu->GetSettingCommandTextId(TEXT("ScrollDown")).IsEmpty());
 TestEqual(TEXT("Main page exposes reset prompt"),Menu->GetSettingCommandTextId(TEXT("ResetPage")),FString(TEXT("UI_Settings_ResetPage")));
 State->SetValue(TEXT("Display.AdvancedData"),0);
 Menu->SelectedSettingId=TEXT("Display.AdvancedData"); Menu->ConfirmSelection();
 TestEqual(TEXT("Choice confirm does not change its value"),State->GetInteger(Menu->SelectedSettingId),0);
 Menu->AdjustSelectedValue(1); TestEqual(TEXT("Choice increase changes the index"),State->GetInteger(Menu->SelectedSettingId),1);
 Menu->SelectedSettingId=TEXT("Controller.InvertCameraX"); State->SetValue(Menu->SelectedSettingId,0);
 Menu->AdjustSelectedValue(1); TestEqual(TEXT("Increase cannot toggle a switch"),State->GetInteger(Menu->SelectedSettingId),0);
 Menu->ConfirmSelection(); TestEqual(TEXT("Confirm toggles a switch"),State->GetInteger(Menu->SelectedSettingId),1);
 Menu->SelectedSettingId=TEXT("MouseKeyboard.CameraSensitivity"); State->SetValue(Menu->SelectedSettingId,66);
 Menu->ConfirmSelection(); TestEqual(TEXT("Slider confirm does not change its value"),State->GetInteger(Menu->SelectedSettingId),66);
 Menu->AdjustSelectedValue(-1); TestEqual(TEXT("Slider decrease changes the position"),State->GetInteger(Menu->SelectedSettingId),65);
 TestTrue(TEXT("Slider has no Confirm prompt"),Menu->GetSettingCommandTextId(TEXT("Confirm")).IsEmpty());
 TestEqual(TEXT("Slider keeps decrease label"),Menu->GetSettingCommandTextId(TEXT("AdjustLeft")),FString(TEXT("UI_Settings_AdjustLeft")));
 Menu->ExecuteSettingsAction(TEXT("AdjustRight"),false);
 Menu->TickSliderAdjustment(0.2f,false,true);
 TestEqual(TEXT("Slider waits before repeating"),State->GetInteger(Menu->SelectedSettingId),66);
 Menu->TickSliderAdjustment(0.2f,false,true);
 TestEqual(TEXT("Held slider increases after delay"),State->GetInteger(Menu->SelectedSettingId),67);
 Menu->TickSliderAdjustment(0.08f,false,true);
 TestEqual(TEXT("Held slider continues increasing"),State->GetInteger(Menu->SelectedSettingId),68);
 Menu->TickSliderAdjustment(1.f,false,false);
 TestEqual(TEXT("Release stops slider repeat"),State->GetInteger(Menu->SelectedSettingId),68);
 Menu->ExecuteSettingsAction(TEXT("AdjustLeft"),false); Menu->TickSliderAdjustment(0.36f,true,false);
 TestEqual(TEXT("Held slider also decreases"),State->GetInteger(Menu->SelectedSettingId),66);
 Menu->TickSliderAdjustment(1.f,true,true);
 TestEqual(TEXT("Opposite held inputs cancel repeat"),State->GetInteger(Menu->SelectedSettingId),66);
 Menu->SelectedSettingId=TEXT("Display.AdvancedData");
 TestEqual(TEXT("Choice uses previous item label"),Menu->GetSettingCommandTextId(TEXT("AdjustLeft")),FString(TEXT("UI_Settings_PreviousChoice")));
 TestEqual(TEXT("Choice uses next item label"),Menu->GetSettingCommandTextId(TEXT("AdjustRight")),FString(TEXT("UI_Settings_NextChoice")));
 TestTrue(TEXT("Choice has no Confirm prompt"),Menu->GetSettingCommandTextId(TEXT("Confirm")).IsEmpty());
 Menu->TickSliderAdjustment(1.f,true,false);
 TestEqual(TEXT("Held adjustment never repeats Choice"),State->GetInteger(Menu->SelectedSettingId),1);
 for (FName Id:{FName(TEXT("Controller.InvertCameraX")),FName(TEXT("Controller.KeyBindingsMenu"))})
 {
  Menu->SelectedSettingId=Id;
  TestTrue(TEXT("Toggle and submenu hide decrease"),Menu->GetSettingCommandTextId(TEXT("AdjustLeft")).IsEmpty());
  TestTrue(TEXT("Toggle and submenu hide increase"),Menu->GetSettingCommandTextId(TEXT("AdjustRight")).IsEmpty());
  TestEqual(TEXT("Toggle and submenu show Confirm"),Menu->GetSettingCommandTextId(TEXT("Confirm")),FString(TEXT("UI_Settings_Confirm")));
 }
 State->CurrentValues.Add(TEXT("Game.Language"),2);
 Menu->Rows.Reset();
 for (const FName Id:{FName(TEXT("Game.Language")),FName(TEXT("MouseKeyboard.CameraSensitivity"))})
 {
  const auto* Definition=State->FindDefinition(Id);
  const auto RowClass=Menu->RowWidgetClasses.FindChecked(Definition->Type);
  auto* Row=CreateWidget<UTASettingRowWidget>(PC,RowClass); Row->Configure(*Definition,FText::GetEmpty(),FText::GetEmpty(),State->GetInteger(Id),false);
  Menu->Rows.Add(Row);
  if (auto* Button=Cast<UButton>(Row->GetWidgetFromName(TEXT("Button_Option"))))
   TestTrue(TEXT("Setting buttons have no pressed darkening"),Button->GetStyle().Pressed==Button->GetStyle().Normal);
 }
 Menu->RestoreCurrentDefaults();
 TestEqual(TEXT("Page reset preserves the selected language"),State->GetInteger(TEXT("Game.Language")),2);
 TestEqual(TEXT("Page reset still restores other settings"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),State->FindDefinition(TEXT("MouseKeyboard.CameraSensitivity"))->DefaultValue);
 Menu->bGamepadDevice=true; Menu->BeginGamepadNavigation();
 TestFalse(TEXT("Gamepad navigation hides the pointer"),Menu->ShouldShowPlayerCursor());
 Menu->InputPresentationChanged(); TestFalse(TEXT("Other gamepad prompt updates keep navigation mode"),Menu->ShouldShowPlayerCursor());
 Menu->bGamepadDevice=false; Menu->InputPresentationChanged(); TestTrue(TEXT("Keyboard switch restores the pointer"),Menu->ShouldShowPlayerCursor());
 auto* Context=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Input/IMC_Settings.IMC_Settings"));
 if (!TestNotNull(TEXT("Authored settings IMC"),Context)) return false;
 // The user's authored keys are editable. Give the remapping test a known
 // keyboard key in a transient fixture without changing the asset.
 Context=DuplicateObject<UInputMappingContext>(Context,GetTransientPackage());
 auto* PageAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/Input/Actions/Settings/IA_SettingsNextPage.IA_SettingsNextPage"));
 if (!TestNotNull(TEXT("Next page action"),PageAction)) return false;
 Context->MapKey(PageAction,EKeys::PageDown);
 auto* ResetAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/Input/Actions/Settings/IA_SettingsResetPage.IA_SettingsResetPage"));
 if (!TestNotNull(TEXT("Reset page action"),ResetAction)) return false;
 Context->MapKey(ResetAction,EKeys::R); Context->MapKey(ResetAction,EKeys::Gamepad_FaceButton_Top);
 FSettingsTestInput Input; Input.Input=NewObject<UTAPlayerInput>(PC); PC->PlayerInput=Input.Input;
 for (const auto& M:Context->GetMappings())
 {
  PC->SettingsActions.Add(FName(*M.Action->GetName().RightChop(11)),const_cast<UInputAction*>(M.Action.Get()));
  TestTrue(TEXT("Settings actions execute while paused"),M.Action->bTriggerWhenPaused);
 }
 Input.AddMappingContext(Context,20); FModifyContextOptions Options; Options.bForceImmediately=true; Input.RequestRebuildControlMappings(Options);
 TestFalse(TEXT("Keyboard Up cannot select the previous setting"),Menu->ResolvePlayerInput(EKeys::Up).IsSet());
 TestFalse(TEXT("Keyboard Enter is not settings Confirm"),Menu->ResolvePlayerInput(EKeys::Enter).IsSet());
 TestTrue(TEXT("Gamepad item navigation comes from IMC"),Menu->ResolvePlayerInput(EKeys::Gamepad_DPad_Up).IsSet());
 TestTrue(TEXT("Keyboard page navigation comes from IMC"),Menu->ResolvePlayerInput(EKeys::PageDown).IsSet());
 TestTrue(TEXT("Keyboard reset comes from IMC"),Menu->ResolvePlayerInput(EKeys::R).IsSet());
 TestTrue(TEXT("Gamepad reset comes from IMC"),Menu->ResolvePlayerInput(EKeys::Gamepad_FaceButton_Top).IsSet());
 TestTrue(TEXT("Gamepad confirm comes from IMC"),Menu->ResolvePlayerInput(EKeys::Gamepad_FaceButton_Bottom).IsSet());
 // Move an authored key: routing must follow the remapped IMC, without a hardcoded fallback.
 auto* Remapped=DuplicateObject<UInputMappingContext>(Context,GetTransientPackage());
 auto* NextPage=PC->GetSettingsAction(TEXT("NextPage")); Remapped->UnmapKey(NextPage,EKeys::PageDown); Remapped->MapKey(NextPage,EKeys::P);
 Input.RemoveMappingContext(Context); Input.AddMappingContext(Remapped,20); Input.RequestRebuildControlMappings(Options);
 TestFalse(TEXT("Removed PageDown no longer navigates"),Menu->ResolvePlayerInput(EKeys::PageDown).IsSet());
 TestTrue(TEXT("Remapped page key navigates"),Menu->ResolvePlayerInput(EKeys::P).IsSet());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsResetHoldTest,"TheAwakening.Settings.ResetHold",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsResetHoldTest::RunTest(const FString&)
{
 FTASettingsResetHold Hold;
 TestFalse(TEXT("Short hold does not reset"),Hold.Tick(1.8f,true));
 TestTrue(TEXT("Hold fill is linear"),FMath::IsNearlyEqual(Hold.Display,0.6f,1.e-5f));
 Hold.Tick(0,false);
 Hold.Tick(0.1f,false);
 TestTrue(TEXT("Release uses a cubic return"),FMath::IsNearlyEqual(Hold.Display,0.6f*0.9f*0.9f*0.9f,1.e-5f));
 const float BeforeResume=Hold.Display;
 TestFalse(TEXT("Repress does not immediately reset"),Hold.Tick(0,true));
 TestTrue(TEXT("Repress preserves the returning visual"),FMath::IsNearlyEqual(Hold.Display,BeforeResume));
 Hold.Tick(0.1f,true);
 TestTrue(TEXT("Old return wins while larger"),FMath::IsNearlyEqual(Hold.Display,0.6f*0.8f*0.8f*0.8f,1.e-5f));
 TestFalse(TEXT("Old visual does not shorten the new hold"),Hold.Tick(2.8f,true));
 TestTrue(TEXT("Fresh uninterrupted three seconds triggers"),Hold.Tick(0.11f,true));
 TestEqual(TEXT("Completion begins return from full"),Hold.Display,1.f);
 TestFalse(TEXT("Completed hold returns without releasing"),Hold.Tick(0.5f,true));
 TestTrue(TEXT("Automatic return is fast then slow"),FMath::IsNearlyEqual(Hold.Display,0.125f));
 Hold.Tick(0,false);
 TestTrue(TEXT("Release during completion return preserves its timeline"),FMath::IsNearlyEqual(Hold.ReturnSeconds,0.5f));
 Hold.Tick(0.5f,false); TestEqual(TEXT("Return reaches zero at one second"),Hold.Display,0.f);
 TestTrue(TEXT("Releasing permits another full hold"),Hold.Tick(3.f,true));
 TestFalse(TEXT("Continued hold cannot repeatedly reset"),Hold.Tick(4.f,true));
 TestEqual(TEXT("Continued hold stays empty after automatic return"),Hold.Display,0.f);
 Hold.Tick(0,false); TestEqual(TEXT("Release clears accumulated hold time"),Hold.HoldSeconds,0.f);
 FTASettingsResetHold Custom;
 TestFalse(TEXT("Custom duration waits two seconds"),Custom.Tick(1.f,true,2.f,0.5f));
 TestEqual(TEXT("Custom duration controls the fill fraction"),Custom.Display,0.5f);
 TestTrue(TEXT("Custom duration triggers after two seconds"),Custom.Tick(1.f,true,2.f,0.5f));
 Custom.Tick(0.25f,true,2.f,0.5f);
 TestEqual(TEXT("Custom return duration controls ease-out"),Custom.Display,0.125f);
 Custom.Tick(0.25f,true,2.f,0.5f);
 TestEqual(TEXT("Custom return finishes in half a second"),Custom.Display,0.f);
 return true;
}
#endif
