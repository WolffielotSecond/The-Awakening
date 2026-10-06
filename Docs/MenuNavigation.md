# Menu Back Navigation

The controller routes the key mapped to the dedicated `IA_UIBack` action in `IMC_UI` to the current winning `ITAPlayerInputReceiver`. Each menu handles one back step through `HandleMenuBackRequested()`. `IA_Pause` remains responsible for opening the pause menu from gameplay.

## Current behavior

- Pause menu: closes the pause menu and resumes play.
- Inventory: closes the inventory panel.
- Dialogue history: closes the history overlay and returns to the dialogue.
- Dialogue: consumes back input without closing; the dialogue ends through its normal story flow.
- Path puzzle: closes the puzzle and ends its active session.

Key repeats are consumed without invoking another back step, so holding the back key cannot close multiple nested menus.

## Adding a child menu

Keep the parent menu alive while its child is open. Give the child an input request with a higher priority than the parent and implement `HandleMenuBackRequested()` on the child to close itself. Releasing the child's request makes the still-open parent the input winner again, so the next back press returns to that parent.

When a menu can only be opened directly, its back handler should close that menu. No navigation history is inferred: the parent/child relationship comes from which menu widgets remain active and own input requests.

Settings child pages should follow the same pattern when implemented. At present, the pause menu's Settings option does not open a settings page.
