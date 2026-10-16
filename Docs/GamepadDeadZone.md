# Gamepad dead zones

## Rule

Each stick and trigger has a minimum (inner) and maximum (outer) dead zone,
both 1%-45%. With magnitude `v` (stick: radial length of X/Y; trigger: travel):

- `v < min`: ignored. The value is 0 and the input never switches the active
  input device (prompt icons, menu device mode).
- `v == min` maps to 0; `v >= 1 - max` maps to 1; linear in between:
  `(v - min) / (1 - max - min)`. Sticks keep their direction.

Defaults: stick min 20%, everything else 5%.

## Settings

Controller page, section `Settings.Section.Controller.DeadZone`, eight sliders in
preview order: left trigger, left stick, right stick, right trigger, each min then max
(`Controller.<Input>DeadZoneMin|Max`). They use `bStepByPhysicalValue`, so every
adjustment moves exactly 1%. The rows exist in `FTASettingsCatalog::Build` and in
`/Game/Setting/DA_Settings` (the asset replaces the catalog at runtime); the section is
also in `DA_SettingsPages`.

## Ingress (single source of truth)

`FTAGamepadDeadZone` (Core) holds the raw value of every stick axis and trigger and
the ranges. `AThe_AwakeningPlayerController::RefreshGamepadDeadZones` reads the applied
settings once per frame from the input processor tick.

- Slate analog path (`ObserveAnalogInput`): filtered values are recorded as held input,
  and the paired stick axis is re-recorded because the radial result changes both axes.
  Ignored input does not call `NotifyRawInputKey`.
- Enhanced Input path (`InputKey`): the filtered value replaces `AmountDepressed`; the
  paired axis is re-sent. Ignored input does not notify the device either.
- Engine virtual keys (`Gamepad_LeftStick_Up`..., `Gamepad_LeftTrigger`/`RightTrigger`)
  use fixed engine thresholds. A press inside the player's dead zone is consumed;
  the first repeat after leaving it is delivered as the press (Slate processor and
  Enhanced Input both), and its release is delivered normally.
- Removed duplicates: `InputModifierDeadZone` on IMC_Default (IA_Move, IA_Look) and
  IMC_UI (IA_PuzzlePan), the cursor's hard-coded 0.18, and the character's
  `StickDeadZone` default (now 0, kept as an optional start threshold).
- Disconnect clears the raw dead-zone state.

## Preview

Description block type `DeadZonePreview`. The settings menu inserts
`DeadZonePreviewWidgetClass` (`WBP_DeadZonePreview`, parent
`UTADeadZonePreviewWidget`) into `Box_Description` in place of an image block. One
preview shows four inputs: left trigger, left stick, right stick, right trigger.

Layout and look are authored in WBP_DeadZonePreview. C++ only drives these optional
images every frame (`<Input>` = LeftTrigger, LeftStick, RightStick, RightTrigger); a
missing image or a non-material brush logs one warning (`LogTADeadZonePreview`):

| Image | Driven by |
|---|---|
| `Image_<Input>_Background` | not driven; stick radius = half its width |
| `Image_<Trigger>_MinZone`, `_MaxZone` | material `FillAmount` = min / max (or Y render scale, `TriggerZoneDrive`) |
| `Image_<Trigger>_Fill` | material `FillAmount` = raw travel |
| `Image_<Stick>_MinZone` | uniform render scale = min (or material, `StickMinZoneDrive`) |
| `Image_<Stick>_MaxZone` | ring material `FillAmount` = max (ring width) |
| `Image_<Stick>_Dot` | render translation = raw input x radius (up is up) |

The parameter name is `FillParameterName` (default `FillAmount`). Values use the
menu's draft sliders, so the red areas follow them before Apply; raw input comes from
the owning player controller's `FTAGamepadDeadZone`. Render-scale pivots and fill
directions (the max band fills from the top) are set in the WBP / materials.

## Tests

`TheAwakening.Input.DeadZone.{Math,Settings,Preview,Ingress}`; updated
`TheAwakening.Input.VirtualCursorAxes` and `HeldObservationLifecycle` to the radial
player dead zone.
