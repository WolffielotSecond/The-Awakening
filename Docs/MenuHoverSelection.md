# Hover Selection for Navigable Menus

Selectable menu rows should derive from `UTASelectableMenuOptionWidget`. Its
`OnHovered` event fires whenever the pointer enters the row. The menu should update
its selected index and highlight from that event, without activating the option.
Clicking or pressing Confirm remains the separate activation step.

`UTAPauseMenuOptionWidget` and `UTADialogueChoiceButton` use this shared row base.
The pause menu subscribes to each row's `OnHovered` event and updates its selection;
dialogue choices use the same event to update the active branch choice. This gives
future vertical menus the same mouse-hover behavior while keeping each menu in charge
of its own data, selection, and action handling.

When adding a menu, derive its row widget from `UTASelectableMenuOptionWidget`, bind
each created row's `OnHovered` event to the menu, update the selected row in the
handler, and refresh the row highlights. Do not run the menu action from the hover
handler.
