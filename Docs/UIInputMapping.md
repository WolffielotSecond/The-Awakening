# Shared UI Input Mapping

All modal UI input bindings live in `/Game/Input/IMC_UI`. It contains the mappings
merged from `IMC_Inventory` and `IMC_Dialogue`, plus back and puzzle actions (28 mappings total). The pause menu
reuses the dialogue choice actions `IA_ChoicePrevious`, `IA_ChoiceNext`, and
`IA_ChoiceConfirm`, so it needs no separate `IMC_PauseMenu`.

`AThe_AwakeningPlayerController` loads `IMC_UI` as its default `UIInputMappingContext`.
The Blueprint property remains editable for projects that need to override the default.
The controller adds the context only while a UI input receiver owns input, then removes
it when control returns to gameplay. This keeps the UI actions from consuming overlapping
gameplay keys while no UI is open. Inventory and dialogue keep their existing action
references; neither widget adds its own mapping context. The input router directs each
key to the current UI owner.

Back has its own action, `IA_UIBack`, mapped to Escape and Gamepad Special Right in
`IMC_UI`. The controller routes that action to the current receiver's
`HandleMenuBackRequested()` while the UI context is active. There is no hardcoded Escape
key check. `IA_Pause` remains mapped in `IMC_Default` and opens the pause menu from gameplay.

## Current action groups

- Shared back: `IA_UIBack` (keyboard Escape and Gamepad Special Right).
- Inventory: previous page, next page, confirm, and gamepad drag mode.
- Dialogue: advance, history, choice previous, choice next, and choice confirm.
- Puzzle: `IA_PuzzleConfirm`, `IA_PuzzleUndo`, and `IA_PuzzlePan` (Axis2D).
  Pan maps WASD and the left stick; its stick mapping applies a radial dead zone.
- Pause menu: previous option, next option, and confirm, reusing the dialogue choice
  actions. `UTAPauseMenuWidget` supplies these action defaults from the shared action
  assets.

Enable **Trigger When Paused** on `IA_ChoicePrevious`, `IA_ChoiceNext`, and
`IA_ChoiceConfirm`, so the pause menu can navigate while the world is paused.
`IA_Pause` remains in `IMC_Default` because it opens and closes the pause menu.

For a future UI, add its Input Actions and key bindings to `IMC_UI`, then assign those
actions to its widget defaults. Do not create a separate widget-specific mapping
context.

The former `IMC_Inventory` and `IMC_Dialogue` assets remain as migration references;
gameplay code no longer adds them.

## Unified action prompts

`UTAInputIconSubsystem::OnInputPromptsChanged` refreshes prompts when the input
device, language, or resolved Enhanced Input mappings change. The controller
rebuilds UI mappings immediately and sends this notification when activating
`IMC_UI`, so a newly opened menu can show its keys on its first frame.

`UTAActionPromptWidget` subscribes to this event and resolves its assigned action
using its owning local player. Pause, inventory, and dialogue use
`FTAPromptWidgetUtils::AddActionPrompt` to create the same prompt widget in their
Blueprint horizontal boxes. Keep placing those boxes manually in the Blueprint.

For future menus, assign an Input Action and localized text ID to each
`UTAActionPromptWidget`. No per-menu delayed refresh is needed. Custom inline
prompts subscribe to `OnInputPromptsChanged` and use `GetIconForActionForPlayer`
or `GetPromptKeysForAction`; they should not keep a separate hardcoded key list.
Unmapped actions hide their key icon for the current device.
