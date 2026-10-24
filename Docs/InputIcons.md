# Input prompt icons and gamepad brand

## Scope

Prompt icons support keyboard/mouse, Xbox and PlayStation (DualSense/DualShock).
Switch controllers are not supported and show Xbox prompts. Gameplay input is
unchanged: XInput + Enhanced Input + the project input router. Steam Input is
only queried for the controller type; it never handles actions.

## Detection chain

1. `FTAInputDeviceDetector` (Slate pre-processor) and `InputKey` forward every key
   to `UTAInputIconSubsystem::NotifyInputKey`.
2. XInput dispatches each pad inside an `FInputDeviceScope` whose
   `HardwareDeviceHandle` is the XInput user index. `NotifyInputKey` reads it, so the
   player controller passes nothing extra.
3. On a gamepad key the subsystem resolves the slot's brand:
   - switching from keyboard/mouse to a gamepad, or input from another slot, checks
     immediately;
   - a known brand is cached per slot;
   - an unknown result retries on later gamepad input, at most every
     `GamepadBrandRetryInterval` seconds (Config, default 0.5);
   - any device connection change clears the cache.
4. `FTAGamepadBrandResolver`:
   - Steam (Win64, standalone/packaged only, not in the editor process):
     `SteamInput()->Init(false)`, then `GetControllerForGamepadIndex(slot)` and
     `GetInputTypeForHandle`. PS3/PS4/PS5 -> PlayStation, other types -> Xbox. A slot
     with no Steam handle is a physical XInput pad -> Xbox. Shutdown releases the
     Steam Input API and the shared Steam client handle.
   - Without Steam pads: raw-input HID vendor ids (Sony 0x054C). Valve virtual pads
     are ignored; mixed brands are ambiguous -> Unknown.
5. Unknown brands use Xbox prompts.

`RefreshCurrentDeviceForUser` (player controller BeginPlay) uses the same path with
no slot, querying every connected pad.

## Controller style setting

`Controller.IconStyle` (Controller page, Bindings section): `Auto` (default),
`Xbox`, `DualSense`. Applied by `FTASettingsApplyService` through
`UTAInputIconSubsystem::SetGamepadIconStyle`. Xbox/DualSense override detection;
keyboard/mouse prompts are never affected. Changes broadcast `OnInputPromptsChanged`.

The runtime list comes from `/Game/Setting/DA_Settings`, which replaces the C++
catalog entirely, so the row exists in both `FTASettingsCatalog::Build` and the
asset. Do not run `PopulateDefaultDefinitions` on the asset: it resets every row.

## Icon lookup

`/Game/Textures/Keys/{Keyboard_Mouse|Xbox|PS}/...`. A missing PlayStation icon falls
back to the same key's Xbox icon (`bFallbackToXboxIcons`, Config). Loaded textures
are cached with strong references; missing paths are remembered and logged once
(`LogTAInputIcon`). The cache is released in `Deinitialize`.

## Reserved direction artwork

The former directional parkour added `LS_UpLeft`, `LS_DownLeft`, `LS_UpRight`,
`LS_DownRight` under both Xbox and PS. These duplicated LS placeholders and the
generic chord widget are retained, but parkour no longer uses direction chords.
Current parkour prompts show one resolved action icon per landing: Upper defaults
to Space/B and Lower to Ctrl/A. Both follow the player's actual IMC bindings.

**待办：后续需要绘制并替换四个正式的 LS 斜方向按键图标。** Xbox 和 PS
目前各有一套同名占位资产；替换纹理内容即可，不需要改资源路径。

## Tests

- `TheAwakening.Input.GamepadBrand.Mapping` - Steam type and vendor id mapping.
- `TheAwakening.Input.GamepadBrand.Detection` - switch/slot/cache/retry rules, style
  override, icon cache, Switch -> Xbox icons.
- `TheAwakening.Input.GamepadBrand.Setting` - catalog definition.
- `TheAwakening.Input.IconResources` - every runtime icon path loads.

Manual matrix (logs: `LogTAGamepadBrand`, `LogTAInputIcon`): Xbox / DualSense under
(a) Steam launch with Steam Input on, (b) Steam Input off, (c) editor PIE. Check
that calling `SteamInput()->Init` causes no duplicated or lost input.
