# Pause Menu

The pause menu is a C++ `UTAPauseMenuWidget` whose visual layout is authored in UMG. Its option rows are generated from the widget's editable `Options` array.

## Widget Blueprint setup

1. Create `WBP_PauseMenuOption` with parent class `UTAPauseMenuOptionWidget`.
2. Add a `Button` named `Button_Option` and a child `TextBlock` named `Text_Option`. Optionally add `Image_Highlight` for a custom selected border. Style and size the row in this widget.
3. Create `WBP_PauseMenu` with parent class `UTAPauseMenuWidget`.
4. Arrange the background, title, and other visuals manually. Add a `VerticalBox` named `Box_Options` where the generated rows should appear. Add a `HorizontalBox` named `HorizontalBox_Controls` at the bottom for the generated input prompts.
5. In the `WBP_PauseMenu` class defaults, set `OptionWidgetClass` to `WBP_PauseMenuOption`. The `Options` array defaults to Resume, Settings, and Quit Game; reorder or relabel these entries there.
6. Assign `WBP_PauseMenu` to `PauseMenuWidgetClass` on `BP_ThirdPersonPlayerController`.

The C++ widget builds one styled row for every valid option definition. `OptionId` must be unique. Labels use the project's localization JSON through `LocalizationId`; `Label` is the fallback if the localization entry is unavailable.

## Input and behavior

- `AThe_AwakeningPlayerController` binds `PauseAction` with Enhanced Input on the `Started` event. Create a Boolean `IA_Pause`, map Escape and the controller Menu/Start button to it in `IMC_Default`, and assign it to `PauseAction` on `BP_ThirdPersonPlayerController`.
- Enable **Trigger When Paused** on `IA_Pause` so the same action can close the menu and resume the world.
- The pause widget defaults to `IA_ChoicePrevious`, `IA_ChoiceNext`, and `IA_ChoiceConfirm`; these are also used by dialogue choices and already have mappings in `IMC_UI` (Up/D-pad Up, Down/D-pad Down, Enter/Gamepad Face Button Bottom).
- `AThe_AwakeningPlayerController` loads `/Game/Input/IMC_UI` as the default `UIInputMappingContext` and activates it while a UI input receiver owns input. It is removed when control returns to gameplay, so UI actions do not consume gameplay keys in the background. The Blueprint property remains editable; the pause widget does not manage the mapping context itself.
- The action opens the pause menu during gameplay and closes it while the pause menu is open.
- While paused, the configured IMC changes the highlighted option and confirms it. The same Input Actions drive the bottom prompts and their device icons.
- `HorizontalBox_Controls` is optional. When present, the widget creates Previous Option, Next Option, Select, and Close Menu prompts using `WBP_ActionPrompt` and refreshes their icons when the input device changes.
- The menu pauses the world, acquires a high-priority UI input request, and focuses the first generated button. Closing it releases that request and unpauses the world.
- Pause input can interrupt scanning, but does not stack the menu over another modal UI such as dialogue, inventory, or the puzzle.
- Resume closes the pause menu. Quit Game calls Unreal's `QuitGame` function.
- Settings broadcasts `OnSettingsRequested`. Bind this event in Blueprint when the settings screen is ready; the pause widget does not invent a placeholder settings screen.

The shared context migration for Inventory and Dialogue is documented in `Docs/UIInputMapping.md`.

## Localization entries

`UI_Pause_Resume`, `UI_Pause_QuitGame`, and the four pause input prompt labels are defined in `Content/Localization/zh-CN.json`, `zh-TW.json`, and `en.json`. The Settings row reuses `UI_Settings`.
