# Global UMG font

## Single configuration source

`/Game/Fonts/TA_GlobalFont` is a runtime Composite Font referencing the existing,
unmodified official FontFace assets in `/Game/Fonts/HarmonyOS`:

| Language | Preferred face |
| --- | --- |
| English / default | HarmonyOS_Sans |
| zh-CN / zh-Hans / zh-SG | HarmonyOS_Sans_SC |
| zh-TW / zh-Hant / zh-HK / zh-MO | HarmonyOS_Sans_TC |

Culture-specific ranges cover Unicode. SC is the last-resort Composite fallback:
TC-supported characters remain TC; a character absent from TC (e.g. U+8FC7 `过`)
falls back to SC. This does not guarantee support outside the official fonts' cmaps.

All entries use original face index 0 / default weight 400. `Regular` is the
authored UI style. `Default` and `Bold` are compatibility aliases of Regular,
because UE 5.6 TextBlock's native constructor requests `Bold`. They do not select
weight 706 or synthesize bold. Regular replacing the previous Bold is an accepted
visual change. No size compensation is applied.

`[SlateStyle] DefaultFontName` in `DefaultEngine.ini` sets the project font for new
UMG TextBlock / editable text constructors. Restart the Editor after changing it:
native constructors cache their font lookup. It does not override serialized
fonts, arbitrary Slate style sets, or explicit future SetFont calls.

Future font changes should edit the Composite resource and its referenced faces.
Runtime TextBlock code should copy GetFont(), change only required appearance
fields, and avoid replacing FontObject with an engine font. Editor-only Slate
tools keep FAppStyle typography; they are outside game UMG scope.

## Language synchronization

`UTALocalizeSubsystem::SetLanguage()` remains the sole game language entry. It
validates the UE culture, loads the existing JSON, sets UE's **Language**, updates
CurrentLanguage and broadcasts OnLanguageChanged. It does not change **Locale**,
so date/number formatting keeps its existing locale. Slate uses CurrentLanguage
for Composite subfont selection and invalidates the appropriate font caches when
language changes. Existing widgets keep the same FontObject.

Language is UE-process-wide: multiple PIE game instances cannot display different
font cultures simultaneously. This uses UE's native culture model, without a
second language state or font manager.

Packaging stages en, zh-CN and zh-TW. ICU uses the built-in EFIGSCJK preset, the
smallest available UE preset containing both English and Chinese; English-only
data is insufficient for the new runtime Chinese culture selection. This does
not add selectable game languages or change the JSON language list.

## Migration and validation

24 Widget Blueprints were inspected; 38 Roboto TextBlocks in 18 blueprints were
migrated through native Editor APIs to TA_GlobalFont / Regular. Size, outline,
shadow, colour and all other FontInfo fields were preserved; non-font Blueprint
export content was checked unchanged before compilation/save. Backups and logs
are in ignored Saved/FontMigrationBackups and Saved/Logs/GlobalFont*. Native
reloading compiled all 24 blueprints successfully with 38 unified TextBlocks.

Affected blueprints:

- UI/Dialogue: WBP_Dialogue, WBP_DialogueChoiceButton, WBP_DialogueHistory
- UI/Inventory: WBP_InventoryPanel, WBP_InventorySlot
- UI/Minigame: WBP_TestPuzzle
- UI/Pause: WBP_PauseMenuOption
- UI/Scan: WBP_ScanInfo
- UI/Setting: WBP_SettingChoice, WBP_SettingSlider, WBP_SettingSubmenu,
  WBP_SettingToggle, WBP_SettingsDescriptionBlock, WBP_SettingsMenu,
  WBP_SettingsPage, WBP_SettingsSection
- UI: WBP_ActionPrompt, WBP_InteractPrompt

Game runtime SetFont paths only adjust existing sizes (Puzzle/Scope/ActionPrompt);
they preserve the unified font. The assets and game source have no explicit
remaining Roboto references. UE engine/editor fonts themselves are unchanged.
One-shot migration/render commandlets are removed after execution.

Regression tests: TheAwakening.Fonts.LanguagePreservesLocale and
TheAwakening.Fonts.CultureRoutingAndDefault. Real GPU captures of the saved font
are Saved/GlobalFont_en.png, GlobalFont_zh-CN.png and GlobalFont_zh-TW.png.
The full project suite has 30/31 successes. The existing Puzzle.Scope test fails
in headless input/confirmation/timer assertions identically with the pre-migration
puzzle asset; no existing assertion or gameplay behaviour was changed to hide it.

PIE validation remains required: switch all three languages with UI already open;
check Scan, Inventory, Dialogue/History, Puzzle, Pause and Settings; confirm long
text wrapping/clipping and outline/shadow readability. Different font metrics can
alter text width/height even when size and layout are preserved. Create a fresh
TextBlock in a scratch WBP after Editor restart and confirm the project font.
Interactive UI, packaged font cooking and notice staging are not yet validated.

## Distribution and attribution

`Content/ThirdPartyNotices/HarmonyOS/LICENSE-update.txt` is a byte-identical copy
of the supplied original license, including `Copyright 2021 Huawei Device Co.,
Ltd.`. `DirectoriesToAlwaysStageAsNonUFS=ThirdPartyNotices` retains it in packaged
distributions. Verify the actual staged build before release.

**Outstanding release requirement:** add a prominent, player-visible notice in
the game's About/Credits page, reachable from Pause/Settings (WBP_SettingsMenu is
the existing entry point). Suggested text: “This game uses HarmonyOS Sans Fonts.
Copyright 2021 Huawei Device Co., Ltd.” Keep this visible rather than only in a
shipping log or hidden package. No About/Credits UI was added in this font-only
migration; existing layouts and interactions were intentionally preserved.

Do not instantiate variable fonts into modified files, subset/edit their tables,
or alter the original font components. The supplied license must remain with the
fonts; comply with its original terms when distributing the game.
