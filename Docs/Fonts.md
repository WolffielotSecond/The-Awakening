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

In same-process PIE, SetCurrentLanguage also refreshes Editor localization.
The installed UE 5.6 Editor has zh-Hans resources but no zh-Hant/zh-TW resources:
zh-CN changes the Editor to Simplified Chinese, while zh-TW falls back to native
English; en also displays English. Neither this subsystem nor the inspected PIE
lifecycle restores the prior Language on Stop. This is an accepted limitation,
not an editor preference intentionally saved by the game. UE Game Localization
Preview isolates game text resources, but not Composite Font culture selection,
which still reads the process-wide Language. Separate-process Standalone or a
packaged executable provides process isolation. No workaround is implemented.

Packaging configuration requests en, zh-CN and zh-TW. ICU uses the built-in EFIGSCJK preset, the
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
The latest completed full project suite has 33/34 successes (23 successful and
10 successful with warnings); the new Dialogue.HistoryEnglishLayout regression
passes, including language-event refresh, Chinese defaults, spacing and font
appearance preservation. Settings.EnglishLayout and Settings.EnglishUIBounds
also pass. Editor Development compilation and History Widget Blueprint
compilation/loading passed; git diff --check passed. These are completed prior
verification results, not a new build or test run during documentation closeout.
The existing Puzzle.Scope test fails
in headless input/confirmation/timer assertions identically with the pre-migration
puzzle asset; no existing assertion or gameplay behaviour was changed to hide it.

Manual acceptance reported by the user:

| Area | Accepted result / scope |
| --- | --- |
| Global font / language switching | HarmonyOS Sans and English, Simplified Chinese, Traditional Chinese switching work normally. |
| Settings | English layout basically normal; Dialogue text speed is normal in Standalone Game and PIE New Editor Window. Selected Viewport exception is recorded below. |
| Pause Menu | Resume Game fits the button; three-language switching accepted. |
| DialogueHistory | English long-text wrapping, entry spacing and scrolling show no obvious problems. |
| Inventory | English item names show no current problem; no further layout changes requested. |

This acceptance does not include gamepad UI navigation adaptation or exhaustive
coverage at every resolution. It does not establish packaged font cooking,
Culture data completeness, notice staging or packaged UI behavior.

## English settings layout

The existing Settings language refresh applies a local English layout to
WBP_SettingsMenu, WBP_SettingsPage and the Choice/Slider/Toggle/Submenu row
templates. Page labels wrap with content-driven height and 12x8 content padding.
Row names use constrained Fill space, wrapping and a minimum 24-unit gap to the
authored Auto controls; controls are vertically centered. Footer buttons use a
minimum 16-unit gap and 12x8 content padding.

The Slider template has a stretch Canvas under a fixed-height SizeBox. English
therefore reserves at least three font lines plus the actual vertical container
insets and 12-unit text padding above/below, restoring the authored height for
Chinese. The value, arrows and slider keep their Auto widths and remain centered.
English Pause options use the widest current label's desired size plus side
padding to size the shared VBox; all options fill that width without wrapping.
The authored Chinese Canvas offsets and button padding are restored on language
change. Neither adjustment runs in Tick or changes input handling.

Only the affected instance layout properties are retained from the Designer
defaults, so switching back to zh-CN/zh-TW restores their original values.
There is no layout polling, second language state, font override or asset
hierarchy migration. Font, size, letter spacing and localization text are unchanged.
The English layout regression checks the real six WBP templates and round-trip
restoration. Settings/Pause wrapping, alignment and three-language switching have
passed manual PIE acceptance; Settings gamepad navigation was outside that scope.

## English dialogue history layout

WBP_DialogueHistory's dynamic history entries enable AutoWrapText only for the
existing `en` game language. Their VerticalBox slots fill the current ScrollBox
width, keep Auto height, and use an 8-unit bottom gap between entries (no extra
gap after the last entry). Rebuild is already driven by history updates and the
localization language-change event; no layout Tick or input changes are added.
Chinese entries are recreated with the original TextBlock/slot defaults, so
English wrapping and padding cannot leak into zh-CN/zh-TW after a language switch.
Font, size, letter spacing, text resolution and speaker/choice prefixes are unchanged.

Settings/Pause English layout has passed manual three-language PIE acceptance.
History English long-text wrapping, spacing and scrolling have passed manual PIE
acceptance; package validation remains deferred.

## Distribution and attribution

`Content/ThirdPartyNotices/HarmonyOS/LICENSE-update.txt` is a byte-identical copy
of the supplied original license, including `Copyright 2021 Huawei Device Co.,
Ltd.`. `DirectoriesToAlwaysStageAsNonUFS=ThirdPartyNotices` is configured to stage
it in packaged distributions. Actual output and license distribution have not
been verified; check the staged build before release.

**Outstanding release requirement:** add a prominent, player-visible notice in
the game's About/Credits page, reachable from Pause/Settings (WBP_SettingsMenu is
the existing entry point). Suggested text: “This game uses HarmonyOS Sans Fonts.
Copyright 2021 Huawei Device Co., Ltd.” Keep this visible rather than only in a
shipping log or hidden package. No About/Credits UI was added in this font-only
migration; existing layouts and interactions were intentionally preserved.

Do not instantiate variable fonts into modified files, subset/edit their tables,
or alter the original font components. The supplied license must remain with the
fonts; comply with its original terms when distributing the game.

## Closeout and outstanding work

Font migration and the scoped English UI layout work are complete and accepted.
Do not continue altering accepted Settings, Pause, History or Inventory layouts
as part of this task. No Windows package was built or validated for this work.

| Outstanding item | Priority / next step |
| --- | --- |
| Windows Development packaging and font/Culture/license output verification | Deferred to the complete packaging workflow, together with Wwise audio integration. Check original font faces, TA_GlobalFont loading, three-language switching and staged notices in the actual build. |
| Prominent in-game HarmonyOS Sans Credits/About/Licenses notice | Required before release; not implemented. Preserve the usage statement, Copyright 2021 Huawei Device Co., Ltd. and original LICENSE-update.txt. The final UI location remains to be agreed. |
| Selected Viewport Dialogue text speed text compression | Low priority. Three lines work in Standalone and PIE New Editor Window. Investigate viewport size, DPI scale and layout timing only with evidence; no Selected Viewport special case. Raise priority if the packaged version reproduces at the same window size. |
| Same-process PIE changes Editor Language | Confirmed, accepted limitation; no fix planned now. Independent-process testing isolates language state. |
| Puzzle.Scope headless failure | Pre-existing issue, outside font migration; not fixed or hidden by changing assertions. |

Future packaged acceptance must include different window sizes/resolutions and
Settings, Pause, History, Inventory and Scan font/layout checks. Configuration
and editor/Standalone acceptance are not substitutes for packaged validation.
