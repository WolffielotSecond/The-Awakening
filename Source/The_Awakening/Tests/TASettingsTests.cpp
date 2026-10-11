#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Settings/TASettingsTypes.h"
#include "Settings/TASettingsSubsystem.h"
#include "UI/TACommonDialog.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/Pause/TAPauseMenuWidget.h"
#include "UI/Pause/TAPauseMenuOptionWidget.h"
#include "HAL/PlatformTime.h"
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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Settings/TADisplaySettingsService.h"

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
  for (const TCHAR* Id:{TEXT("UI_Settings_ScrollDescriptionUp"),TEXT("UI_Settings_ScrollDescriptionDown"),TEXT("UI_Settings_ScrollListUp"),TEXT("UI_Settings_ScrollListDown")})
   TestTrue(TEXT("Description scroll prompt exists in each language"),Object->HasField(Id) && !Object->GetStringField(Id).IsEmpty());
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
 TestFalse(TEXT("Dialog and settings may reuse keys"),UTAKeyBindingService::GroupsOverlap(TEXT("Dialog"),TEXT("Settings")));
 TestFalse(TEXT("Dialog and shared menu are exclusive"),UTAKeyBindingService::GroupsOverlap(TEXT("Dialog"),TEXT("Menu")));
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
 Menu->SettingsSubsystem=State; State->BeginEditing();
 TestTrue(TEXT("Scroll up remains hidden from prompts"),Menu->GetSettingCommandTextId(TEXT("ScrollUp")).IsEmpty());
 TestTrue(TEXT("Scroll down remains hidden from prompts"),Menu->GetSettingCommandTextId(TEXT("ScrollDown")).IsEmpty());
 TestEqual(TEXT("Main page exposes reset prompt"),Menu->GetSettingCommandTextId(TEXT("ResetPage")),FString(TEXT("UI_Settings_ResetPage")));
 State->SetValue(TEXT("Display.AdvancedData"),0);
 Menu->SelectedSettingId=TEXT("Display.AdvancedData"); Menu->ConfirmSelection();
 TestEqual(TEXT("Choice confirm does not change its value"),State->GetEditableInteger(Menu->SelectedSettingId),0);
 Menu->AdjustSelectedValue(1); TestEqual(TEXT("Choice increase changes the index"),State->GetEditableInteger(Menu->SelectedSettingId),1);
 Menu->SelectedSettingId=TEXT("Controller.InvertCameraX"); State->SetValue(Menu->SelectedSettingId,0);
 Menu->AdjustSelectedValue(1); TestEqual(TEXT("Increase cannot toggle a switch"),State->GetEditableInteger(Menu->SelectedSettingId),0);
 Menu->ConfirmSelection(); TestEqual(TEXT("Confirm toggles a switch"),State->GetEditableInteger(Menu->SelectedSettingId),1);
 Menu->SelectedSettingId=TEXT("MouseKeyboard.CameraSensitivity"); State->SetValue(Menu->SelectedSettingId,66);
 Menu->ConfirmSelection(); TestEqual(TEXT("Slider confirm does not change its value"),State->GetEditableInteger(Menu->SelectedSettingId),66);
 Menu->AdjustSelectedValue(-1); TestEqual(TEXT("Slider decrease changes the position"),State->GetEditableInteger(Menu->SelectedSettingId),65);
 TestTrue(TEXT("Slider has no Confirm prompt"),Menu->GetSettingCommandTextId(TEXT("Confirm")).IsEmpty());
 TestEqual(TEXT("Slider keeps decrease label"),Menu->GetSettingCommandTextId(TEXT("AdjustLeft")),FString(TEXT("UI_Settings_AdjustLeft")));
 Menu->ExecuteSettingsAction(TEXT("AdjustRight"),false);
 Menu->TickSliderAdjustment(0.2f,false,true);
 TestEqual(TEXT("Slider waits before repeating"),State->GetEditableInteger(Menu->SelectedSettingId),66);
 Menu->TickSliderAdjustment(0.2f,false,true);
 TestEqual(TEXT("Held slider increases after delay"),State->GetEditableInteger(Menu->SelectedSettingId),67);
 Menu->TickSliderAdjustment(0.08f,false,true);
 TestEqual(TEXT("Held slider continues increasing"),State->GetEditableInteger(Menu->SelectedSettingId),68);
 Menu->TickSliderAdjustment(1.f,false,false);
 TestEqual(TEXT("Release stops slider repeat"),State->GetEditableInteger(Menu->SelectedSettingId),68);
 Menu->ExecuteSettingsAction(TEXT("AdjustLeft"),false); Menu->TickSliderAdjustment(0.36f,true,false);
 TestEqual(TEXT("Held slider also decreases"),State->GetEditableInteger(Menu->SelectedSettingId),66);
 Menu->TickSliderAdjustment(1.f,true,true);
 TestEqual(TEXT("Opposite held inputs cancel repeat"),State->GetEditableInteger(Menu->SelectedSettingId),66);
 Menu->SelectedSettingId=TEXT("Display.AdvancedData");
 TestEqual(TEXT("Choice uses previous item label"),Menu->GetSettingCommandTextId(TEXT("AdjustLeft")),FString(TEXT("UI_Settings_PreviousChoice")));
 TestEqual(TEXT("Choice uses next item label"),Menu->GetSettingCommandTextId(TEXT("AdjustRight")),FString(TEXT("UI_Settings_NextChoice")));
 TestTrue(TEXT("Choice has no Confirm prompt"),Menu->GetSettingCommandTextId(TEXT("Confirm")).IsEmpty());
 Menu->TickSliderAdjustment(1.f,true,false);
 TestEqual(TEXT("Held adjustment never repeats Choice"),State->GetEditableInteger(Menu->SelectedSettingId),1);
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
  auto* Row=CreateWidget<UTASettingRowWidget>(PC,RowClass); Row->Configure(*Definition,FText::GetEmpty(),FText::GetEmpty(),State->GetEditableInteger(Id),false);
  Menu->Rows.Add(Row);
  if (auto* Button=Cast<UButton>(Row->GetWidgetFromName(TEXT("Button_Option"))))
   TestTrue(TEXT("Setting buttons have no pressed darkening"),Button->GetStyle().Pressed==Button->GetStyle().Normal);
  if (auto* Title=Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Name"))))
  {
   const auto Before=Title->GetRenderTransform(); Row->SetDirty(true);
   TestTrue(TEXT("Dirty titles are italic even without an italic font face"),Title->GetRenderTransform().Shear.X!=Before.Shear.X);
   Row->SetDirty(false); TestTrue(TEXT("Clean title restores authored transform"),Title->GetRenderTransform()==Before);
  }
 }
 Menu->RestoreCurrentDefaults();
 TestEqual(TEXT("Page reset preserves the selected language"),State->GetEditableInteger(TEXT("Game.Language")),2);
 TestEqual(TEXT("Page reset still restores other settings"),State->GetEditableInteger(TEXT("MouseKeyboard.CameraSensitivity")),State->FindDefinition(TEXT("MouseKeyboard.CameraSensitivity"))->DefaultValue);
 TestEqual(TEXT("Page reset immediately applies defaults"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),State->FindDefinition(TEXT("MouseKeyboard.CameraSensitivity"))->DefaultValue);
 TestFalse(TEXT("Restored page has no unsaved reset draft"),State->IsSettingDirty(TEXT("MouseKeyboard.CameraSensitivity")));
 const FName InitialPage=Menu->CurrentPageId;
 State->SetDraftValue(TEXT("Controller.InvertCameraX"),1);
 Menu->PendingExit=UTASettingsMenuWidget::EPendingExit::Page; Menu->PendingTarget=TEXT("Settings.Page.Display");
 Menu->DialogResult(TEXT("Cancel"));
 TestEqual(TEXT("Cancel leave keeps current page"),Menu->CurrentPageId,InitialPage);
 TestTrue(TEXT("Cancel leave preserves draft"),State->HasDraftChanges());
 Menu->PendingExit=UTASettingsMenuWidget::EPendingExit::Page; Menu->PendingTarget=TEXT("Settings.Page.Display");
 Menu->DialogResult(TEXT("Discard"));
 TestFalse(TEXT("Discard on leave clears drafts"),State->HasDraftChanges());
 TestEqual(TEXT("Discard continues requested page switch"),Menu->CurrentPageId,FName(TEXT("Settings.Page.Display")));
 State->SetDraftValue(TEXT("Controller.InvertCameraX"),1);
 Menu->PendingExit=UTASettingsMenuWidget::EPendingExit::Page; Menu->PendingTarget=TEXT("Settings.Page.Controller");
 Menu->DialogResult(TEXT("Apply"));
 TestTrue(TEXT("Apply on leave updates runtime"),State->GetBoolean(TEXT("Controller.InvertCameraX")));
 TestEqual(TEXT("Apply continues requested page switch"),Menu->CurrentPageId,FName(TEXT("Settings.Page.Controller")));
 State->OnVideoModeResolved.AddUniqueDynamic(Menu,&UTASettingsMenuWidget::VideoResolved);
 for (bool Keep:{true,false})
 {
  State->PreviousDisplayState=FTADisplaySettingsService::GetCurrentState(FTADisplaySettingsService::GetMonitors());
  State->VideoDeadline=FPlatformTime::Seconds()+15;
  Menu->PendingExit=UTASettingsMenuWidget::EPendingExit::Page; Menu->PendingTarget=Keep?TEXT("KeepExit"):TEXT("RevertExit"); Menu->bVideoDialog=true;
  Menu->DialogResult(Keep?TEXT("Keep"):TEXT("Revert"));
  TestEqual(TEXT("Keep and revert both continue the requested exit"),Menu->CurrentPageId,FName(Keep?TEXT("KeepExit"):TEXT("RevertExit")));
  TestTrue(TEXT("Video choice preserves applied non-video settings"),State->GetBoolean(TEXT("Controller.InvertCameraX")));
 }
 State->OnVideoModeResolved.RemoveDynamic(Menu,&UTASettingsMenuWidget::VideoResolved);
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
 TestTrue(TEXT("Keyboard Enter is settings Apply from IMC"),Menu->ResolvePlayerInput(EKeys::Enter).IsSet());
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
 // Two-area navigation executes through the same owner/handle authorization.
 State->DiscardDraft(); State->Pages=FTASettingsCatalog::BuildPages();
 Menu->CurrentPageId=FTASettingsCatalog::FavoritesPageId();
 Menu->SelectionArea=UTASettingsMenuWidget::ESelectionArea::Categories;
 Menu->BuildPageEntries(); Menu->BuildSettingRows();
 Menu->bGamepadDevice=true; Menu->BeginGamepadNavigation();
 FTAInputRequest Request; Request.Owner=Menu; Request.Priority=600;
 Request.Allowed={ETAInputCapability::Navigate,ETAInputCapability::Confirm,ETAInputCapability::Close,ETAInputCapability::ToggleFavorite};
 Menu->SetPlayerInputRequest(PC,PC->AcquireInputRequest(Request));
 TestEqual(TEXT("Main menu starts on Favorites"),Menu->CurrentPageId,FTASettingsCatalog::FavoritesPageId());
 // Left-column reset remains an IMC command and commits only the displayed page.
 Menu->CurrentPageId=TEXT("Settings.Page.MouseKeyboard"); Menu->BuildSettingRows();
 State->SetValue(TEXT("MouseKeyboard.CameraSensitivity"),66);
 State->SetValue(TEXT("Controller.InvertCameraX"),1); State->SetDraftValue(TEXT("Controller.InvertCameraX"),0);
 TestEqual(TEXT("Sidebar includes reset prompt"),Menu->GetSettingCommandTextId(TEXT("ResetPage")),FString(TEXT("UI_Settings_ResetPage")));
 PC->RecordHeldInput(EKeys::Gamepad_FaceButton_Top,1.f,Local->GetControllerId());
 TestTrue(TEXT("Y reset routes from sidebar"),PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Top,false,Local->GetControllerId()));
 Menu->TickResetHold(1.f);
 TestEqual(TEXT("Short sidebar hold does not reset"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),66);
 Menu->TickResetHold(2.f);
 TestEqual(TEXT("Sidebar hold applies page defaults"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),State->FindDefinition(TEXT("MouseKeyboard.CameraSensitivity"))->DefaultValue);
 TestTrue(TEXT("Sidebar reset preserves navigation area"),Menu->SelectionArea==UTASettingsMenuWidget::ESelectionArea::Categories);
 TestTrue(TEXT("Sidebar reset leaves another page draft pending"),State->GetBoolean(TEXT("Controller.InvertCameraX")) && State->IsSettingDirty(TEXT("Controller.InvertCameraX")));
 State->DiscardDraft();
 TestEqual(TEXT("Discard cannot undo applied reset"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),State->FindDefinition(TEXT("MouseKeyboard.CameraSensitivity"))->DefaultValue);
 State->SetValue(TEXT("MouseKeyboard.CameraSensitivity"),66);
 Menu->TickResetHold(4.f);
 TestEqual(TEXT("One continuous hold cannot reset twice"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),66);
 PC->RecordHeldInput(EKeys::Gamepad_FaceButton_Top,0.f,Local->GetControllerId()); Menu->TickResetHold(0.f);
 PC->RecordHeldInput(EKeys::Gamepad_FaceButton_Top,1.f,Local->GetControllerId());
 PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Top,false,Local->GetControllerId()); Menu->TickResetHold(3.f);
 TestEqual(TEXT("Release and press permits another reset"),State->GetInteger(TEXT("MouseKeyboard.CameraSensitivity")),State->FindDefinition(TEXT("MouseKeyboard.CameraSensitivity"))->DefaultValue);
 PC->RecordHeldInput(EKeys::Gamepad_FaceButton_Top,0.f,Local->GetControllerId()); Menu->TickResetHold(0.f);
 Menu->CurrentPageId=FTASettingsCatalog::FavoritesPageId(); Menu->BuildSettingRows();
 TestEqual(TEXT("Sidebar has category confirmation prompt"),Menu->GetSettingCommandTextId(TEXT("Confirm")),FString(TEXT("UI_Settings_EnterCategory")));
 TestTrue(TEXT("Sidebar has no adjustment prompt"),Menu->GetSettingCommandTextId(TEXT("AdjustRight")).IsEmpty());
 TestTrue(TEXT("LB removed from category navigation"),!Menu->ResolvePlayerInput(EKeys::Gamepad_LeftShoulder).IsSet());
 TestTrue(TEXT("RB removed from category navigation"),!Menu->ResolvePlayerInput(EKeys::Gamepad_RightShoulder).IsSet());
 TestTrue(TEXT("Down routes to current menu"),PC->RoutePlayerInputKey(EKeys::Gamepad_DPad_Down,false,Local->GetControllerId()));
 TestEqual(TEXT("Down in sidebar chooses Game"),Menu->CurrentPageId,FName(TEXT("Settings.Page.Game")));
 TestTrue(TEXT("A enters details"),PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Bottom,false,Local->GetControllerId()));
 TestTrue(TEXT("Details region selected"),Menu->SelectionArea==UTASettingsMenuWidget::ESelectionArea::Details);
 const FName DetailPage=Menu->CurrentPageId;
 PC->RoutePlayerInputKey(EKeys::Gamepad_DPad_Down,false,Local->GetControllerId());
 TestEqual(TEXT("Down in details never changes category"),Menu->CurrentPageId,DetailPage);
 const auto* Slider=State->FindDefinition(TEXT("Game.DialogueTextSpeed"));
 if (TestNotNull(TEXT("Real Game slider"),Slider))
 {
  Menu->SelectedSettingId=Slider->SettingId; Menu->EnsureRowSelection();
  const int32 Before=State->GetEditableInteger(Slider->SettingId);
  PC->RoutePlayerInputKey(EKeys::Gamepad_DPad_Right,false,Local->GetControllerId());
  TestTrue(TEXT("Right in details adjusts draft"),State->GetEditableInteger(Slider->SettingId)!=Before);
  // A real popup requires a LocalPlayer subsystem in interactive play. Exercise
  // the same result continuation without pretending this headless fixture owns one.
  Menu->PendingExit=UTASettingsMenuWidget::EPendingExit::Categories;
  Menu->DialogResult(TEXT("Discard"));
  TestFalse(TEXT("B discard clears draft"),State->HasDraftChanges());
  TestTrue(TEXT("Discard returns to categories"),Menu->SelectionArea==UTASettingsMenuWidget::ESelectionArea::Categories);
  TestEqual(TEXT("Returning retains the selected category"),Menu->CurrentPageId,DetailPage);
  Menu->ExecuteSettingsAction(TEXT("Confirm"),true);
  State->SetDraftValue(TEXT("Controller.InvertCameraX"),0);
  Menu->PendingExit=UTASettingsMenuWidget::EPendingExit::Categories;
  Menu->DialogResult(TEXT("Apply"));
  TestFalse(TEXT("A applies draft to runtime"),State->GetBoolean(TEXT("Controller.InvertCameraX")));
  TestTrue(TEXT("Apply returns to categories"),Menu->SelectionArea==UTASettingsMenuWidget::ESelectionArea::Categories);
  Menu->ExecuteSettingsAction(TEXT("Confirm"),true);
  TestTrue(TEXT("B returns clean details without a popup"),Menu->HandleMenuBackRequested());
  TestTrue(TEXT("Clean B selects categories"),Menu->SelectionArea==UTASettingsMenuWidget::ESelectionArea::Categories && !Menu->ActiveDialog);
 }
 // RS targets the next column, based on logical selection rather than pointer hit testing.
 // Physical keys still resolve through the current IMC and exact owner authorization.
 auto* DescriptionScroll=Menu->WidgetTree->ConstructWidget<UScrollBox>();
 auto* LongContent=Menu->WidgetTree->ConstructWidget<USizeBox>(); LongContent->SetHeightOverride(600.f);
 DescriptionScroll->AddChild(LongContent);
 Menu->ScrollBox_Description=DescriptionScroll;
 const auto SlateDescription=StaticCastSharedRef<SScrollBox>(DescriptionScroll->TakeWidget());
 SlateDescription->SlatePrepass();
 const FGeometry DescriptionGeometry=FGeometry::MakeRoot(FVector2D(300,100),FSlateLayoutTransform());
 SlateDescription->Tick(DescriptionGeometry,0,0);
 TestTrue(TEXT("Description fixture has real scrollable overflow"),DescriptionScroll->GetScrollOffsetOfEnd()>100.f);
 auto* OptionsScroll=Menu->WidgetTree->ConstructWidget<UScrollBox>();
 auto* LongOptions=Menu->WidgetTree->ConstructWidget<USizeBox>(); LongOptions->SetHeightOverride(600.f);
 OptionsScroll->AddChild(LongOptions); Menu->ScrollBox_SettingsOptions=OptionsScroll;
 const auto SlateOptions=StaticCastSharedRef<SScrollBox>(OptionsScroll->TakeWidget());
 SlateOptions->SlatePrepass(); SlateOptions->Tick(DescriptionGeometry,0,0);
 Menu->SelectedSettingId=TEXT("MouseKeyboard.CameraSensitivity");
 Menu->bGamepadDevice=true;
 Menu->BuildPromptBar();
 TestTrue(TEXT("Categories target only the middle list"),Menu->GetScrollTarget()==OptionsScroll);
 TestEqual(TEXT("Categories show list scrolling hint"),Menu->GetSettingCommandTextId(TEXT("ScrollDown")),FString(TEXT("UI_Settings_ScrollListDown")));
 PC->RoutePlayerInputKey(EKeys::Gamepad_RightStick_Down,false,Local->GetControllerId());
 TestEqual(TEXT("RS does not jump immediately"),OptionsScroll->GetScrollOffset(),0.f);
 Menu->TickScrollInterpolation(0.016f);
 TestTrue(TEXT("RS interpolation advances partway"),OptionsScroll->GetScrollOffset()>0.f && OptionsScroll->GetScrollOffset()<60.f);
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("Categories scroll middle list"),OptionsScroll->GetScrollOffset(),60.f);
 TestEqual(TEXT("Categories cannot scroll description"),DescriptionScroll->GetScrollOffset(),0.f);
 PC->RecordHeldInput(EKeys::Gamepad_RightStick_Down,1.f,Local->GetControllerId());
 Menu->NativeTick(DescriptionGeometry,0.36f);
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("Held RS in categories repeats on middle list"),OptionsScroll->GetScrollOffset(),120.f);
 TestEqual(TEXT("Held RS in categories cannot touch description"),DescriptionScroll->GetScrollOffset(),0.f);
 PC->RecordHeldInput(EKeys::Gamepad_RightStick_Down,0.f,Local->GetControllerId());
 PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Bottom,false,Local->GetControllerId());
 TestTrue(TEXT("A moves RS target to description"),Menu->GetScrollTarget()==DescriptionScroll);
 TestEqual(TEXT("Gamepad up scroll hint is localized"),Menu->GetSettingCommandTextId(TEXT("ScrollUp")),FString(TEXT("UI_Settings_ScrollDescriptionUp")));
 TestEqual(TEXT("Gamepad down scroll hint is localized"),Menu->GetSettingCommandTextId(TEXT("ScrollDown")),FString(TEXT("UI_Settings_ScrollDescriptionDown")));
 Menu->bGamepadDevice=false;
 TestTrue(TEXT("Keyboard hides description scroll hints"),Menu->GetSettingCommandTextId(TEXT("ScrollUp")).IsEmpty() && Menu->GetSettingCommandTextId(TEXT("ScrollDown")).IsEmpty());
 Menu->bGamepadDevice=true;
 const float OptionsOffset=Menu->ScrollBox_SettingsOptions?Menu->ScrollBox_SettingsOptions->GetScrollOffset():0.f;
 TestTrue(TEXT("RS down routed through IMC"),PC->RoutePlayerInputKey(EKeys::Gamepad_RightStick_Down,false,Local->GetControllerId()));
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("RS down scrolls description without pointer hit testing"),DescriptionScroll->GetScrollOffset(),60.f);
 PC->RoutePlayerInputKey(EKeys::Gamepad_RightStick_Up,false,Local->GetControllerId());
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("RS up scrolls description back"),DescriptionScroll->GetScrollOffset(),0.f);
 PC->RecordHeldInput(EKeys::Gamepad_RightStick_Down,1.f,Local->GetControllerId());
 Menu->NativeTick(DescriptionGeometry,0.36f);
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("Held RS continues description scrolling"),DescriptionScroll->GetScrollOffset(),60.f);
 PC->RecordHeldInput(EKeys::Gamepad_RightStick_Down,0.f,Local->GetControllerId());
 Menu->NativeTick(DescriptionGeometry,0.36f);
 TestEqual(TEXT("Real RS release stops scrolling"),DescriptionScroll->GetScrollOffset(),60.f);
 PC->RoutePlayerInputKey(EKeys::Gamepad_RightStick_Up,false,Local->GetControllerId());
 Menu->TickScrollInterpolation(1.f);
 if (Menu->ScrollBox_SettingsOptions) TestEqual(TEXT("RS does not scroll settings list"),Menu->ScrollBox_SettingsOptions->GetScrollOffset(),OptionsOffset);
 const auto ScrollHint=[&]()->UTAActionPromptWidget*
 {
  for (UTAActionPromptWidget* Prompt:Menu->Prompts)
   if (Prompt && Prompt->GetPromptAction()==PC->GetSettingsAction(TEXT("ScrollDown"))) return Prompt;
  return nullptr;
 };
 auto* Hint=ScrollHint();
 if (TestNotNull(TEXT("Scroll hint exists after layout"),Hint))
 {
  TestTrue(TEXT("Scrollable description shows hint"),Hint->GetVisibility()==ESlateVisibility::Visible);
  // Resize so the same content fits: no hidden fallback to the middle column.
  SlateDescription->Tick(FGeometry::MakeRoot(FVector2D(300,1000),FSlateLayoutTransform()),0,0);
  Menu->RefreshScrollPromptVisibility();
  TestFalse(TEXT("Description without overflow cannot scroll"),Menu->CanScrollCurrentRegion());
  TestTrue(TEXT("Description without overflow hides hint"),Hint->GetVisibility()==ESlateVisibility::Collapsed);
  PC->RoutePlayerInputKey(EKeys::Gamepad_RightStick_Down,false,Local->GetControllerId());
  TestEqual(TEXT("Non-scrollable description remains still"),DescriptionScroll->GetScrollOffset(),0.f);
  TestEqual(TEXT("Non-scrollable description does not scroll middle list instead"),OptionsScroll->GetScrollOffset(),OptionsOffset);
  SlateDescription->Tick(DescriptionGeometry,0,0); Menu->RefreshScrollPromptVisibility();
  TestTrue(TEXT("Overflow restored by resizing shows hint again"),Hint->GetVisibility()==ESlateVisibility::Visible);
 }
 PC->RecordHeldInput(EKeys::Gamepad_RightStick_Down,1.f,Local->GetControllerId());
 TestTrue(TEXT("B returns to categories"),Menu->HandleMenuBackRequested());
 TestTrue(TEXT("B restores middle-list target"),Menu->GetScrollTarget()==OptionsScroll);
 Menu->NativeTick(DescriptionGeometry,0.36f);
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("Held RS after B scrolls only middle list"),OptionsScroll->GetScrollOffset(),OptionsOffset+60.f);
 TestEqual(TEXT("B cannot leave held scrolling on description"),DescriptionScroll->GetScrollOffset(),0.f);
 PC->RecordHeldInput(EKeys::Gamepad_RightStick_Down,0.f,Local->GetControllerId());
 SlateOptions->Tick(FGeometry::MakeRoot(FVector2D(300,1000),FSlateLayoutTransform()),0,0);
 Menu->RefreshScrollPromptVisibility();
 TestFalse(TEXT("Categories without list overflow cannot scroll"),Menu->CanScrollCurrentRegion());
 if (auto* ListHint=ScrollHint()) TestTrue(TEXT("Categories without list overflow hide hint"),ListHint->GetVisibility()==ESlateVisibility::Collapsed);
 SlateOptions->Tick(DescriptionGeometry,0,0);
 FTAInputRequest Cover; Cover.Owner=PC; Cover.Priority=1000; Cover.Allowed={ETAInputCapability::Navigate,ETAInputCapability::Confirm};
 const auto CoverHandle=PC->AcquireInputRequest(Cover);
 const FName CoveredPage=Menu->CurrentPageId;
 Menu->ExecutePlayerInput(EKeys::Gamepad_DPad_Down,ETAInputCapability::Navigate);
 TestEqual(TEXT("Covered menu cannot navigate with another winner's permission"),Menu->CurrentPageId,CoveredPage);
 Menu->ExecutePlayerInput(EKeys::Gamepad_RightStick_Down,ETAInputCapability::Navigate);
 TestEqual(TEXT("Covered menu cannot scroll description"),DescriptionScroll->GetScrollOffset(),0.f);
 PC->ReleaseInputRequest(CoverHandle);
 // Changing the mapped key changes behavior; the removed physical key has no fallback.
 auto* NextItem=PC->GetSettingsAction(TEXT("NextItem"));
 Remapped->UnmapKey(NextItem,EKeys::Gamepad_DPad_Down); Remapped->MapKey(NextItem,EKeys::Gamepad_LeftTrigger);
 Input.RequestRebuildControlMappings(Options);
 TestFalse(TEXT("Removed Down no longer resolves"),Menu->ResolvePlayerInput(EKeys::Gamepad_DPad_Down).IsSet());
 TestTrue(TEXT("Rebound category navigation resolves"),PC->RoutePlayerInputKey(EKeys::Gamepad_LeftTrigger,false,Local->GetControllerId()));
 TestTrue(TEXT("Rebound action moves the selected category"),Menu->CurrentPageId!=CoveredPage);
 Menu->EnterDetails();
 auto* ScrollDown=PC->GetSettingsAction(TEXT("ScrollDown"));
 Remapped->UnmapKey(ScrollDown,EKeys::Gamepad_RightStick_Down); Remapped->MapKey(ScrollDown,EKeys::Gamepad_RightTrigger);
 Input.RequestRebuildControlMappings(Options);
 TestFalse(TEXT("Removed RS down has no hardcoded scroll fallback"),Menu->ResolvePlayerInput(EKeys::Gamepad_RightStick_Down).IsSet());
 TestTrue(TEXT("Remapped scroll action routes"),PC->RoutePlayerInputKey(EKeys::Gamepad_RightTrigger,false,Local->GetControllerId()));
 Menu->TickScrollInterpolation(1.f);
 TestEqual(TEXT("Remapped scroll action scrolls description"),DescriptionScroll->GetScrollOffset(),60.f);
 TestFalse(TEXT("Settings never enables virtual cursor"),Menu->AllowsVirtualCursor());
 TestFalse(TEXT("Settings no longer resolves synthetic pointer clicks"),Menu->ResolvePlayerInput(EKeys::Gamepad_RightThumbstick).IsSet());
 PC->RecordHeldInput(EKeys::Gamepad_Left2D,0.8f,Local->GetControllerId());
 TestTrue(TEXT("Settings LS cannot produce cursor motion"),PC->GetCursorInputAxis().IsNearlyZero());
 const FName PreservedSetting=Menu->SelectedSettingId;
 Menu->NotifyPlayerPointerMoved();
 TestEqual(TEXT("Hidden pointer cannot change selection"),Menu->SelectedSettingId,PreservedSetting);
 Menu->bGamepadDevice=false;
 TestTrue(TEXT("Settings real pointer is visible"),Menu->ShouldShowPlayerCursor());
 TestNotNull(TEXT("Settings supplies selected row pointer target"),Menu->GetPlayerPointerTarget());
 Menu->bGamepadDevice=true;
 TestFalse(TEXT("Settings gamepad pointer is hidden"),Menu->ShouldShowPlayerCursor());
 const float BeforeHandoff=DescriptionScroll->GetScrollOffset();
 Menu->ScrollCurrentRegion(1);
 Menu->SelectionArea=UTASettingsMenuWidget::ESelectionArea::Categories;
 Menu->TickScrollInterpolation(0.1f);
 TestEqual(TEXT("Region change cancels old scroll target"),DescriptionScroll->GetScrollOffset(),BeforeHandoff);
 Menu->SelectionArea=UTASettingsMenuWidget::ESelectionArea::Details;
 Menu->ScrollCurrentRegion(1); Menu->bGamepadDevice=false;
 Menu->TickScrollInterpolation(0.1f);
 TestEqual(TEXT("Real-pointer handoff stops gamepad scroll interpolation"),DescriptionScroll->GetScrollOffset(),BeforeHandoff);
 Menu->ReleaseRequest();
 auto* PauseClass=LoadClass<UTAPauseMenuWidget>(nullptr,TEXT("/Game/UI/Pause/WBP_PauseMenu.WBP_PauseMenu_C"));
 auto* Pause=PauseClass?CreateWidget<UTAPauseMenuWidget>(PC,PauseClass):nullptr;
 if (TestNotNull(TEXT("Pause template"),Pause))
 {
  Pause->BuildOptions(); Pause->bGamepadDevice=true;
  TestFalse(TEXT("Pause disables virtual cursor"),Pause->AllowsVirtualCursor());
  TestFalse(TEXT("Pause gamepad hides cursor"),Pause->ShouldShowPlayerCursor());
  auto* First=Pause->GetPlayerPointerTarget();
  Pause->MoveSelection(1); auto* Selected=Pause->GetPlayerPointerTarget();
  TestTrue(TEXT("Pause pointer return target follows selection"),Selected && Selected!=First);
  if (auto* Option=Pause->OptionWidgetsById.Find(TEXT("Resume"))) Pause->HandleOptionHovered(Option->Get());
  TestTrue(TEXT("Hidden pause pointer cannot override navigation"),Pause->GetPlayerPointerTarget()==Selected);
  Pause->bGamepadDevice=false;
  TestTrue(TEXT("Pause mouse displays cursor without resetting selection"),Pause->ShouldShowPlayerCursor() && Pause->GetPlayerPointerTarget()==Selected);
 }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsDisplayChoicesTest,"TheAwakening.Settings.DisplayChoices",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsDisplayChoicesTest::RunTest(const FString&)
{
 FTADisplayMonitor Primary; Primary.Id=TEXT("MonitorA"); Primary.bPrimary=true; Primary.Desktop=FIntPoint(2560,1440);
 Primary.DisplayRect={0,0,2560,1440}; Primary.WorkArea=Primary.DisplayRect;
 Primary.FullscreenResolutions={FIntPoint(1280,720),FIntPoint(1920,1080),Primary.Desktop};
 FTADisplayMonitor Secondary; Secondary.Id=TEXT("MonitorB"); Secondary.Desktop=FIntPoint(1920,1200);
 Secondary.DisplayRect={-1920,0,0,1200}; Secondary.WorkArea=Secondary.DisplayRect;
 Secondary.FullscreenResolutions={FIntPoint(1280,720),FIntPoint(1920,1080),FIntPoint(1280,800),Secondary.Desktop};
 const TArray<FTADisplayMonitor> Monitors={Primary,Secondary};
 FTADisplayState Current; Current.MonitorId=Primary.Id; Current.Resolution=Primary.Desktop; Current.WindowMode=EWindowMode::Fullscreen;
 FTADisplayState Target;
 TestTrue(TEXT("Monitor change produces a complete target"),FTADisplaySettingsService::BuildChange(TEXT("Display.Monitor"),Secondary.Id,Monitors,Current,Target));
 TestEqual(TEXT("Target monitor identity is retained"),Target.MonitorId,Secondary.Id);
 TestEqual(TEXT("Monitor change keeps aspect when supported"),Target.Resolution,FIntPoint(1920,1080));
 TestTrue(TEXT("Window moves onto a negative-coordinate display"),Target.WindowPosition.X<0);
 TestTrue(TEXT("Aspect change selects a real resolution"),FTADisplaySettingsService::BuildChange(TEXT("Display.AspectRatio"),TEXT("16:10"),Monitors,Target,Current));
 TestEqual(TEXT("Aspect selects the nearest pixel count"),Current.Resolution,Secondary.Desktop);
 TestFalse(TEXT("Unavailable ratio is rejected"),FTADisplaySettingsService::BuildChange(TEXT("Display.AspectRatio"),TEXT("32:9"),Monitors,Current,Target));
 TestFalse(TEXT("Unavailable monitor is rejected"),FTADisplaySettingsService::BuildChange(TEXT("Display.Monitor"),TEXT("Removed"),Monitors,Current,Target));
 TestFalse(TEXT("Unsupported fullscreen resolution is rejected"),FTADisplaySettingsService::BuildChange(TEXT("Display.Resolution"),TEXT("3840x2160"),Monitors,Current,Target));
 TestTrue(TEXT("Borderless mode is available"),FTADisplaySettingsService::BuildChange(TEXT("Display.WindowMode"),TEXT("WindowedFullscreen"),Monitors,Current,Target));
 TestEqual(TEXT("Borderless uses selected desktop"),Target.Resolution,Secondary.Desktop);
 TestEqual(TEXT("Borderless has exactly one resolution"),FTADisplaySettingsService::GetResolutions(Secondary,EWindowMode::WindowedFullscreen).Num(),1);
 FTASettingDefinition Resolution; Resolution.ChoiceProviderId=TEXT("ScreenResolutions");
 FTADisplaySettingsService::RefreshDefinition(Resolution,Monitors,Target);
 TestEqual(TEXT("Borderless resolution row has one candidate"),Resolution.Choices.Num(),1);
 TestEqual(TEXT("Stored resolution is an array index"),Resolution.GetChoiceValue(0),FName(TEXT("1920x1200")));
 Current=Target; Current.WindowMode=EWindowMode::Fullscreen; Current.Resolution=FIntPoint(1920,1080);
 FTADisplaySettingsService::RefreshDefinition(Resolution,Monitors,Current);
 TestTrue(TEXT("Resolution list contains selected ratio"),Resolution.FindChoiceIndex(TEXT("1280x720"))!=INDEX_NONE);
 TestEqual(TEXT("Resolution list excludes other ratios"),Resolution.FindChoiceIndex(TEXT("1920x1200")),INDEX_NONE);
 FTASettingDefinition Aspect; Aspect.ChoiceProviderId=TEXT("DisplayAspectRatios");
 FTADisplaySettingsService::RefreshDefinition(Aspect,Monitors,Current);
 TestTrue(TEXT("Ratios include supported 16:9 and 16:10"),Aspect.FindChoiceIndex(TEXT("16:9"))!=INDEX_NONE && Aspect.FindChoiceIndex(TEXT("16:10"))!=INDEX_NONE);
 const auto Windowed=FTADisplaySettingsService::GetResolutions(Secondary,EWindowMode::Windowed);
 TestTrue(TEXT("Windowed supports common non-fullscreen sizes"),Windowed.Contains(FIntPoint(1024,768)));
 TestFalse(TEXT("Windowed excludes sizes larger than desktop"),Windowed.Contains(FIntPoint(3840,2160)));
 TestEqual(TEXT("Ultrawide marketing sizes share 21:9"),FTADisplaySettingsService::AspectRatio(FIntPoint(3440,1440)),FName(TEXT("21:9")));
 TestEqual(TEXT("Invalid resolution has no ratio"),FTADisplaySettingsService::AspectRatio(FIntPoint(0,0)),NAME_None);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsDraftTest,"TheAwakening.Settings.Drafts",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsDraftTest::RunTest(const FString&)
{
 const FString Filename=FPaths::ProjectSavedDir()/TEXT("Automation/SettingsDraft-Test.ini");
 FFileHelper::SaveStringToFile(TEXT(""),*Filename); GConfig->LoadFile(Filename);
 TGuardValue<FString> File(GGameUserSettingsIni,Filename);
 ON_SCOPE_EXIT { GConfig->UnloadFile(Filename); IFileManager::Get().Delete(*Filename); };
 auto* Local=NewObject<ULocalPlayer>(GEngine);
 auto* S=NewObject<UTASettingsSubsystem>(Local); S->Definitions=FTASettingsCatalog::Build(S);
 const FName Slider(TEXT("MouseKeyboard.CameraSensitivity")),Toggle(TEXT("Controller.InvertCameraX")),Choice(TEXT("Display.AdvancedData"));
 S->SetValue(Slider,50); S->SetValue(Toggle,0); S->SetValue(Choice,0); S->BeginEditing();
 TestTrue(TEXT("Slider stages successfully"),S->SetDraftValue(Slider,75));
 TestEqual(TEXT("Runtime slider remains applied"),S->GetInteger(Slider),50);
 TestEqual(TEXT("Editable slider shows draft"),S->GetEditableInteger(Slider),75);
 TestTrue(TEXT("Draft is marked dirty"),S->IsSettingDirty(Slider));
 S->SetDraftValue(Slider,50); TestFalse(TEXT("Returning to applied value clears dirty"),S->HasDraftChanges());
 S->SetDraftValue(Toggle,1); S->SetDraftValue(Choice,2);
 TestFalse(TEXT("Draft toggle cannot affect runtime"),S->GetBoolean(Toggle));
 TestEqual(TEXT("Runtime choice remains basic index"),S->GetInteger(Choice),0);
 S->SetFavorite(Choice,true);
 auto* Saved=NewObject<UTASettingsSubsystem>(Local); Saved->Definitions=S->Definitions; Saved->LoadSavedValues();
 TestEqual(TEXT("Favorite save never persists choice drafts"),Saved->GetInteger(Choice),0);
 TestTrue(TEXT("Applying drafts succeeds"),S->ApplyDraft());
 TestTrue(TEXT("Applied toggle now affects runtime"),S->GetBoolean(Toggle));
 TestEqual(TEXT("Applied choice retains index semantics"),S->GetInteger(Choice),2);
 TestFalse(TEXT("Apply clears dirty state"),S->HasDraftChanges());
 S->SetDraftValue(Slider,75); S->DiscardDraft();
 TestEqual(TEXT("Discard shows applied slider"),S->GetEditableInteger(Slider),50);
 TestFalse(TEXT("Discard clears dirty state"),S->HasDraftChanges());
 S->StageDefault(Slider); TestEqual(TEXT("Reset stages default"),S->GetEditableInteger(Slider),S->FindDefinition(Slider)->DefaultValue);
 TestEqual(TEXT("Reset leaves runtime slider intact"),S->GetInteger(Slider),50);
 S->DiscardDraft();
 S->SetDraftValue(Slider,75); S->SetDraftValue(Toggle,0); S->StageDefault(Slider);
 TestTrue(TEXT("Scoped default commits without applying the whole session"),S->ApplyDraftForSettings({Slider}));
 const int32 Default=S->FindDefinition(Slider)->DefaultValue;
 TestEqual(TEXT("Scoped reset changes runtime immediately"),S->GetInteger(Slider),Default);
 TestFalse(TEXT("Scoped reset clears its own dirty state"),S->IsSettingDirty(Slider));
 TestTrue(TEXT("Unrelated draft remains pending"),S->GetBoolean(Toggle) && S->IsSettingDirty(Toggle));
 auto* ResetSaved=NewObject<UTASettingsSubsystem>(Local); ResetSaved->Definitions=S->Definitions; ResetSaved->LoadSavedValues();
 TestEqual(TEXT("Scoped default is persisted"),ResetSaved->GetInteger(Slider),Default);
 TestTrue(TEXT("Unrelated draft is not persisted"),ResetSaved->GetBoolean(Toggle));
 S->DiscardDraft();
 TestEqual(TEXT("Discard does not undo completed reset"),S->GetInteger(Slider),Default);
 const auto Before=FTADisplaySettingsService::GetCurrentState(FTADisplaySettingsService::GetMonitors());
 const auto* Mode=S->FindEditableDefinition(TEXT("Display.WindowMode"));
 if (TestNotNull(TEXT("Video mode definition"),Mode))
 {
  const FName ModeId=Mode->SettingId;
  const int32 Current=S->GetEditableInteger(Mode->SettingId);
  TestTrue(TEXT("Video change is staged"),S->SetDraftValue(Mode->SettingId,(Current+1)%Mode->Choices.Num()));
  const auto After=FTADisplaySettingsService::GetCurrentState(FTADisplaySettingsService::GetMonitors());
  TestEqual(TEXT("Staging does not apply screen mode"),After.WindowMode,Before.WindowMode);
  TestEqual(TEXT("Staging does not resize screen"),After.Resolution,Before.Resolution);
  TestFalse(TEXT("Staging has no video confirmation deadline"),S->HasPendingVideoMode());
  S->SetValue(Slider,50); S->StageDefault(Slider);
  TestTrue(TEXT("Reset of a non-video page succeeds with a video draft present"),S->ApplyDraftForSettings({Slider}));
  TestFalse(TEXT("Non-video reset does not start video confirmation"),S->HasPendingVideoMode());
  TestTrue(TEXT("Unrelated video draft remains dirty after reset"),S->IsSettingDirty(ModeId));
  TestEqual(TEXT("Non-video reset does not apply drafted screen mode"),FTADisplaySettingsService::GetCurrentState(FTADisplaySettingsService::GetMonitors()).WindowMode,Before.WindowMode);
  S->RefreshDisplayCandidates(); TestTrue(TEXT("Hardware refresh preserves video draft"),S->IsSettingDirty(ModeId));
 }
 S->DiscardDraft();
 S->PreviousDisplayState=Before; S->VideoDeadline=FPlatformTime::Seconds()-1;
 S->CheckVideoModeTimeout();
 TestFalse(TEXT("Timeout finishes video confirmation"),S->HasPendingVideoMode());
 TestTrue(TEXT("Video revert preserves applied non-video toggle"),S->GetBoolean(Toggle));
 TestEqual(TEXT("Video revert preserves applied advanced HUD"),S->GetInteger(Choice),2);
 S->EndEditing(); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASettingsDialogTest,"TheAwakening.Settings.CommonDialog",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASettingsDialogTest::RunTest(const FString&)
{
 FTACommonDialogDefinition D;
 TestFalse(TEXT("No-button popup is invalid"),D.IsValid());
 for (int32 I=1;I<=4;++I) { FTACommonDialogOption O; O.ResultId=FName(*FString::FromInt(I)); O.LabelTextId=TEXT("Dialog.OK"); O.InputSlot=I; D.Options.Add(O); }
 TestTrue(TEXT("Four buttons are supported"),D.IsValid());
 const FTACommonDialogOption Fifth=D.Options[0];
 D.Options.Add(Fifth); TestFalse(TEXT("Five buttons are rejected"),D.IsValid()); D.Options.Pop();
 D.Options[3].InputSlot=1; TestFalse(TEXT("Duplicate input slot is rejected"),D.IsValid()); D.Options[3].InputSlot=4;
 D.Options[3].ResultId=D.Options[0].ResultId; TestFalse(TEXT("Duplicate result ID is rejected"),D.IsValid()); D.Options[3].ResultId=TEXT("4");
 D.TimeoutSeconds=15; TestFalse(TEXT("Timeout requires a valid result"),D.IsValid()); D.TimeoutResultId=TEXT("3");
 TestTrue(TEXT("Timeout result can map to any button"),D.IsValid());
 auto* IMC=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Input/IMC_CommonDialog.IMC_CommonDialog"));
 if (!TestNotNull(TEXT("Independent popup IMC asset"),IMC)) return false;
 TSet<const UInputAction*> Actions;
 for (const auto& M:IMC->GetMappings()) { Actions.Add(M.Action); TestTrue(TEXT("Popup actions work while paused"),M.Action->bTriggerWhenPaused); }
 TestEqual(TEXT("Popup owns exactly four logical actions"),Actions.Num(),4);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=PCClass?World->SpawnActor<AThe_AwakeningPlayerController>(PCClass):nullptr;
 if (!TestNotNull(TEXT("Dialog test controller"),PC)) return false;
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local; World->AddController(PC);
 auto* DialogClass=LoadClass<UTACommonDialogWidget>(nullptr,TEXT("/Game/UI/Common/WBP_CommonDialog.WBP_CommonDialog_C"));
 auto* Popup=DialogClass?CreateWidget<UTACommonDialogWidget>(PC,DialogClass):nullptr;
 if (!TestNotNull(TEXT("Authored dialog template with native fallback"),Popup)) return false;
 TestNotNull(TEXT("Dialog fallback constructs button one"),Popup->DialogOption_1.Get());
 TestNotNull(TEXT("Dialog fallback constructs button four"),Popup->DialogOption_4.Get());
 TestFalse(TEXT("Dialog does not route shared Back"),Popup->UsesSharedBackAction());
 FSettingsTestInput Input; Input.Input=NewObject<UTAPlayerInput>(PC); PC->PlayerInput=Input.Input;
 for (const auto& M:IMC->GetMappings()) PC->DialogActions.Add(FCString::Atoi(*M.Action->GetName().RightChop(15)),const_cast<UInputAction*>(M.Action.Get()));
 Input.AddMappingContext(IMC,30); FModifyContextOptions Options; Options.bForceImmediately=true; Input.RequestRebuildControlMappings(Options);
 Popup->Data=D; Popup->Controller=PC;
 TestTrue(TEXT("Existing dialogs retain virtual cursor by default"),Popup->AllowsVirtualCursor());
 Popup->Data.bAllowGamepadCursor=false;
 TestFalse(TEXT("Navigation-only dialog disables virtual cursor"),Popup->AllowsVirtualCursor());
 TestNotNull(TEXT("Navigation-only dialog provides a real-pointer return target"),Popup->GetPlayerPointerTarget());
 TestTrue(TEXT("Popup key is resolved from its IMC"),Popup->ResolvePlayerInput(EKeys::Enter).IsSet());
 // Returning from details has two results, using the authored A/B slots.
 Popup->Data.Options={D.Options[0],D.Options[1]};
 TestTrue(TEXT("Details popup accepts mapped A"),Popup->ResolvePlayerInput(EKeys::Gamepad_FaceButton_Bottom).IsSet());
 TestTrue(TEXT("Details popup accepts mapped B"),Popup->ResolvePlayerInput(EKeys::Gamepad_FaceButton_Right).IsSet());
 TestFalse(TEXT("Details popup does not borrow unused X slot"),Popup->ResolvePlayerInput(EKeys::Gamepad_FaceButton_Left).IsSet());
 Popup->Data=D;
 auto* Remapped=DuplicateObject<UInputMappingContext>(IMC,GetTransientPackage());
 Remapped->UnmapKey(PC->GetDialogAction(1),EKeys::Enter); Remapped->MapKey(PC->GetDialogAction(1),EKeys::P);
 Input.RemoveMappingContext(IMC); Input.AddMappingContext(Remapped,30); Input.RequestRebuildControlMappings(Options);
 TestFalse(TEXT("Removed Enter is no longer accepted"),Popup->ResolvePlayerInput(EKeys::Enter).IsSet());
 TestTrue(TEXT("Popup follows remapped key"),Popup->ResolvePlayerInput(EKeys::P).IsSet());
 auto* Underlying=NewObject<UInputMappingContext>(); FTAInputRequest Parent; Parent.Owner=Underlying; Parent.Priority=600;
 const auto ParentHandle=PC->AcquireInputRequest(Parent);
 FTAInputRequest Modal; Modal.Owner=Popup; Modal.Priority=1000; Modal.Allowed={ETAInputCapability::Confirm};
 Popup->Handle=PC->AcquireInputRequest(Modal);
 TestTrue(TEXT("Modal owns input over original screen"),PC->GetInputWinner().Request.Owner.Get()==Popup);
 Popup->Complete(TEXT("Unknown")); TestTrue(TEXT("Unknown result cannot close dialog"),PC->GetInputWinner().Request.Owner.Get()==Popup);
 Popup->Complete(D.Options[0].ResultId);
 TestEqual(TEXT("Completion releases exact modal handle"),PC->GetInputWinner().Handle,ParentHandle);
 Popup->Dismiss(); TestEqual(TEXT("Repeated dismiss leaves original owner intact"),PC->GetInputWinner().Handle,ParentHandle);
 PC->ReleaseInputRequest(ParentHandle);
 return true;
}

#endif
