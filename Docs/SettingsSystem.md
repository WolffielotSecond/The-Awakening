# Settings system

Six pages: Game, Display, Audio, Mouse/Keyboard, Controller, Favorites.
Audio stays empty until Wwise volume consumers are connected. Auto dialogue
advance/skip, FOV, vibration intensity and prompt device selection are excluded.

## Blueprint wiring

All layout, styling, containers, buttons and calibration artwork are manually
authored in UMG. C++ generates only option rows and action prompts.

1. Create a WBP based on UTASettingsMenuWidget and assign it to
   SettingsMenuWidgetClass on BP_ThirdPersonPlayerController.
2. Place VerticalBox Box_SettingsOptions. Place six page buttons:
   Button_Game, Button_Display, Button_Audio, Button_MouseKeyboard,
   Button_Controller, Button_Favorites. Their optional localized TextBlocks:
   Text_Game, Text_Display, Text_Audio, Text_MouseKeyboard, Text_Controller,
   Text_Favorites. Box_Pages from the old draft is no longer used.
3. Optional controls: Text_Title, Text_Description, Text_Empty,
   HorizontalBox_Controls and Button_RestoreDefaults.
4. Place Button_ConfirmVideoMode and Button_RevertVideoMode on the main WBP.
   They appear while a video change is pending. It reverts after 15 real seconds
   (including while paused), or when Settings closes.
5. Create row templates based on UTAToggleSettingRowWidget,
   UTAChoiceSettingRowWidget, UTASliderSettingRowWidget,
   UTASubmenuSettingRowWidget. Each requires Button_Option and Text_Name.
   Optional: Text_Value, Button_Decrease, Button_Increase, Button_Favorite,
   Slider_Value, Image_Highlight, Image_Favorite. Assign RowWidgetClasses by type,
   or one universal template through OptionWidgetClass. Slider rows should have
   a real Slider_Value; C++ sets range, step and binds pointer drag.
6. Create a separate brightness WBP based on UTABrightnessMenuWidget.
   Add Box_SettingsOptions and desired title/description/prompt/default controls.
   Place a dark/mid/light calibration image or swatches manually. Assign
   BrightnessMenuWidgetClass on the main menu. Gamma previews and persists
   immediately; it adjusts SDR display gamma, with default 2.2.
7. Create a separate bindings WBP based on UTAKeyBindingsMenuWidget.
   Add Box_SettingsOptions, optional Text_CaptureStatus and Button_CancelCapture.
   Assign KeyBindingsMenuWidgetClass on the main menu. Distinct menu definitions
   select keyboard/mouse or controller bindings, using the same WBP.
8. Configure row/prompt templates on BOTH submenu WBPs as well as the main WBP.
9. Place a WBP based on UTAAdvancedDataWidget with Text_AdvancedData in your HUD.
   Keep its owner ticking even in Off mode: C++ empties its text. Refresh is
   every 0.25 seconds, including while paused.
10. Use Settings.RestoreDefaults, Settings.ConfirmVideo, Settings.RevertVideo,
    Settings.CancelCapture to localize manually authored button labels.
    Main page/row/prompt text is localized by C++.

The old pause-option template is replaced by dedicated setting row templates.
The WBPs are intentionally not created or positioned by this implementation.

## Definitions, state, application

FTASettingsCatalog builds the default definitions and hardware resolution list.
An optional UTASettingsDefinitionAsset replaces the catalog at startup through
DefaultGame.ini:

```ini
[TheAwakening.PlayerSettings]
DefinitionAsset=/Game/Settings/DA_Settings.DA_Settings
```

Each definition contains SettingId, Page, Type, NameTextId/DescriptionTextId,
DefaultValue, ApplyHandlerId, ApplyPolicy, visibility and type parameters.
Choices store stable values and localized labels. Sliders store range, step,
format, decimal places and optional localized units. Submenus reference
UTASettingsMenuDefinitionAsset: stable menu ID, kind, title and setting IDs.
Custom definitions need an existing or newly implemented C++ ApplyHandlerId.

UTASettingsSubsystem owns validated current values, favorites and notifications.
FTASettingsApplyService owns runtime calls. UGameUserSettings is the sole
persistence authority for engine display values. Custom project values and
favorite IDs use the versioned TheAwakening.PlayerSettings section in
GameUserSettings.ini. Opening widgets never replaces the catalog or reloads
values. Startup does not overwrite saved engine video settings with defaults.

Mouse/gamepad sensitivity and inversion are independent multipliers on authored
camera-offset behavior. Scan input explicitly identifies gamepad input.
Menu cursor speed multiplies the authored speed. Text speed applies to the next
dialogue session. Favorites reference original definitions and values.

## Input and navigation

Pause remains alive at priority 500; Settings uses 600; a child uses 610.
Both RemoveFromParent and NativeDestruct release exact input handles.
Only closing Pause resumes gameplay. Visible definition order determines
navigation. Hover selects; Confirm/click activates.

IMC_UI is still activated only by the controller while UI owns input. The
controller adds paused per-player Input Actions to its transient IMC_UI copy:
AdjustLeft (Left/D-pad Left), AdjustRight (Right/D-pad Right), PreviousPage
(PageUp/left shoulder), NextPage (PageDown/right shoulder), Favorite (F/face left).
Previous/next/confirm and IA_UIBack are reused. Normal menu navigation and
prompts use actions/resolved mappings, not hardcoded keys.

## Rebinding

UTAKeyBindingService creates per-player context copies without editing authored
assets. Digital mappings receive PlayerMappable metadata and are registered
with Enhanced Input User Settings, including inactive UI contexts. Original
actions, triggers and modifiers are preserved. Analog axes stay fixed.
Binding identity combines device, source context, action and authored default
key; do not rename these identities without a save migration.

Device lists, conflicts and restoring defaults are independent. Shared menu
actions overlap specialized UI groups. Gameplay/inventory/dialogue/puzzle
groups can reuse keys on mutually exclusive surfaces.

Capture receives the authorized owner's raw buttons before normal Back/Confirm.
Opening presses are not routed again, repeats are consumed, wrong-device/axis
keys and conflicts report localized errors. Escape or the controller Menu button
always cancels capture; these keys are reserved within the capture screen.
CancelCapture button or external focus loss also cancels without committing.
Successful changes save Enhanced Input profiles and rebuild mappings
immediately, ignoring held keys until release. Unified prompts then refresh.

## Advanced data

Display.AdvancedData has exactly Off, Basic, Full, with default Off.

- Basic: current FPS（frame limit）FPS
- Full: current FPS（frame limit）FPS-version-build ID-quality-language-device

Nonpositive limits show Unlimited. Mixed scalability levels show Custom.
Version reads EngineSettings.GeneralProjectSettings.ProjectVersion.
The build pipeline may stamp DefaultGame.ini:

```ini
[TheAwakening.Build]
BuildId=your-build-id
```

Missing version/build ID shows Unknown; engine version is not a game build ID.

## Verification

TheAwakening.Settings.CatalogValidation checks IDs, definition validity, three
localization catalogs, rejected values, range/relative-step snapping, excluded
features and exactly three advanced-data modes.
TheAwakening.Settings.BindingRoundTrip checks untouched original mappings,
device identities, modifier preservation, resolved controller bindings,
keyboard independence, in-memory profile persistence and conflict groups.
TheAwakening.Settings.StatePersistence checks custom-value save/load, independent
mouse/controller values, ID-based favorites after catalog reordering, invalid
catalog rejection and restoring a single setting. It uses an isolated test INI.

After authoring WBPs, manually verify in PIE: both-device navigation, pointer
sliders, binding capture/cancel/conflicts, restoring device defaults, favorites,
two-level Back/focus, actual resolution confirmation/timeout, brightness
calibration and advanced text placement.
Validate actual window mode/resolution changes in a standalone game window or
packaged build as well. Restart the editor after this native reflection change
before creating the new WBP subclasses.

Implementation verification: Editor Development build succeeded; all three
settings tests and all 15 existing Input tests passed (the headless Input suite
includes fixture warnings). Two native menu fixtures now explicitly configure
the shared UI action references before their Construct hooks.
