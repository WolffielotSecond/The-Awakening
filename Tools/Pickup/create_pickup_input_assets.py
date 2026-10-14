"""Create the pickup selection IMC; preserve any existing user-authored assets."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
folder = "/Game/Input/Actions/Pickup"
context_path = "/Game/Input/IMC_Pickup"

def factory(cls):
    result = unreal.DataAssetFactory()
    result.set_editor_property("data_asset_class", cls)
    return result

if library.does_asset_exist(context_path):
    raise RuntimeError("IMC_Pickup already exists; refusing to overwrite authored mappings.")

context = tools.create_asset("IMC_Pickup", "/Game/Input", unreal.InputMappingContext, factory(unreal.InputMappingContext))
for name, keys in {
    "Previous": ["MouseScrollUp", "Gamepad_DPad_Up"],
    "Next": ["MouseScrollDown", "Gamepad_DPad_Down"],
}.items():
    action_name = "IA_Pickup" + name
    path = folder + "/" + action_name
    action = library.load_asset(path) if library.does_asset_exist(path) else None
    if not action:
        action = tools.create_asset(action_name, folder, unreal.InputAction, factory(unreal.InputAction))
    for key_name in keys:
        key = unreal.Key()
        key.import_text(key_name)
        context.map_key(action, key)
    assert library.save_loaded_asset(action)
assert library.save_loaded_asset(context)
unreal.log("PICKUP_INPUT_ASSETS_SUCCESS")
