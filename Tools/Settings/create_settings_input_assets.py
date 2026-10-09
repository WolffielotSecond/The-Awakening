"""Create editable IMC_Settings and its actions without replacing existing mappings."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
folder = "/Game/Input/Actions/Settings"
mapping_path = "/Game/Input/IMC_Settings"
if library.does_asset_exist(mapping_path):
    raise RuntimeError("IMC_Settings already exists; refusing to overwrite mappings.")
def factory(cls):
    result = unreal.DataAssetFactory()
    result.set_editor_property("data_asset_class", cls)
    return result
mapping = tools.create_asset("IMC_Settings", "/Game/Input", unreal.InputMappingContext, factory(unreal.InputMappingContext))
bindings = {
    "PreviousPage": ["PageUp", "Gamepad_LeftShoulder"],
    "NextPage": ["PageDown", "Gamepad_RightShoulder"],
    "PreviousItem": ["Gamepad_DPad_Up"],
    "NextItem": ["Gamepad_DPad_Down"],
    "AdjustLeft": ["Left", "Gamepad_DPad_Left"],
    "AdjustRight": ["Right", "Gamepad_DPad_Right"],
    "Confirm": ["Gamepad_FaceButton_Bottom"],
    "Favorite": ["F", "Gamepad_FaceButton_Left"],
    "ResetPage": ["R", "Gamepad_FaceButton_Top"],
    "Back": ["Escape", "Gamepad_FaceButton_Right"],
    "ScrollUp": ["Gamepad_LeftTrigger"],
    "ScrollDown": ["Gamepad_RightTrigger"],
    "PointerClick": ["Gamepad_RightThumbstick"],
    "PointerMove": ["Gamepad_Left2D"],
}
for name, keys in bindings.items():
    path = folder + "/IA_Settings" + name
    action = library.load_asset(path) if library.does_asset_exist(path) else None
    if not action:
        action = tools.create_asset("IA_Settings" + name, folder, unreal.InputAction, factory(unreal.InputAction))
    action.set_editor_property("trigger_when_paused", True)
    if name == "PointerMove":
        action.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
        deadzone = unreal.new_object(unreal.InputModifierDeadZone, outer=action)
        action.set_editor_property("modifiers", [deadzone])
    for key in keys:
        mapped_key = unreal.Key()
        mapped_key.import_text(key)
        mapping.map_key(action, mapped_key)
    assert library.save_loaded_asset(action)
assert library.save_loaded_asset(mapping)
unreal.log("SETTINGS_INPUT_ASSETS_SUCCESS: 14 actions in IMC_Settings")
