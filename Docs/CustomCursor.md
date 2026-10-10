# Unified game cursor

The local `AThe_AwakeningPlayerController` installs one procedural Slate software cursor for every visible cursor type. `None` remains hidden. Existing input ownership, cursor visibility and virtual gamepad movement remain the source of truth.

## Try it

1. Build the Development Editor target and run PIE with the project player controller.
2. Open the game console and run `TACursorTest`.
3. Close the console. Use the three targets and large empty area to compare hover and non-hover clicks. Hold, release early, double-click and move during animations.
4. Adjust the five sliders. Set duration greater than interval to see overlapping pulses. Clicking and moving between targets preserve the pulse cadence.
5. Use **Close test** or run `TACursorTest` again to restore the previous input owner.

The controller exposes `CursorStyle` and `HOVER PULSE SETTINGS` to Blueprint defaults and runtime setters. Test sliders change runtime pulse settings only; edit controller Blueprint defaults to persist chosen values.

## Visual and timing defaults

- Radii are ring **inner-edge** radii in relative units: dot 1/3, main ring 5/3.4. At minimum radius the hovered dot has a .4-unit gap.
- One unit starts at 3 Slate units. Main ring stroke is .7 units; pulse stroke .6 units. Radius animation never scales stroke thickness.
- Contraction 100 ms, expansion 160 ms, outward braking up to 50 ms, dot hover transition 120 ms. All are editable.
- Pulse opacity .35, interval 1.2 s, duration .8 s, expansion 2 units.
- First pulse occurs one interval after entering an actionable region. Leaving stops generation; existing pulses finish. Re-entry starts a new interval. Direct movement between actionable targets retains the timer.
- Existing pulses snapshot their radius, opacity, duration, expansion and stroke. Settings changes affect future pulses. Interval changes retain the most recent scheduling anchor; an overdue new interval is scheduled for the next update.

## Click rules

A press begins contraction. Releases before minimum radius do not interrupt it. At minimum radius a held press stays contracted; a released press expands. Repeated presses during contraction or braking do not restart animation. Pressing during expansion preserves both radius and outward velocity, brakes to zero, then contracts. The analytical brake distance is `velocity * duration / 3`; its duration is shortened as necessary to remain below radius 5. No spring, overshoot or hard radius clamp is used.

The animation clock uses platform/UI time and continues while the world is paused. Pointer position is handled by Slate independently of animation. Pulses have no stored screen position and always paint at the same current cursor center. After a stall, missed pulse births are skipped rather than emitted in a burst; the fixed cadence is preserved.

Hiding, losing the player pointer surface or application deactivation clears all visual state immediately. Showing starts clean and derives hover from the current hit path. Teardown restores prior viewport software-cursor mappings.

## Actionable targets

Hover is derived from the actual Slate hit path and `IsInteractable()`, with enabled ancestors required. Standard buttons/sliders/text-entry controls work automatically. Inventory slots additionally require a nonempty draggable item; action prompts require a bound click handler; dialogue backgrounds require permission to advance and a mapped left click.

For future custom UserWidgets handling pointer events, override `NativeIsInteractable()` in C++, or implement the UMG **Is Interactable** event in Blueprint. Return the actual current interaction permission. Informational containers should return false.

## Verification

Automation group: `TheAwakening.Cursor`.

Tests cover short clicks, holds, hover/non-hover equivalence, re-press velocity continuity and radius bounds, no contraction restart, exact pulse radius capture, independent parameter snapshots, overlapping pulses, preserved hover cadence, exit/hide/reset and stalled-frame behavior. The visual test renders the production cursor drawing at representative states for inspection.
