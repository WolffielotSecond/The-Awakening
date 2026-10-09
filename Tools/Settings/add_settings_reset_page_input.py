"""Add ResetPage to the existing settings IMC without changing other mappings."""
import unreal

library = unreal.EditorAssetLibrary
context = library.load_asset('/Game/Input/IMC_Settings')
assert context
path = '/Game/Input/Actions/Settings/IA_SettingsResetPage'
action = library.load_asset(path) if library.does_asset_exist(path) else None
if not action:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.InputAction)
    action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'IA_SettingsResetPage', '/Game/Input/Actions/Settings', unreal.InputAction, factory)
assert action
action.set_editor_property('trigger_when_paused', True)
# Existing ResetPage mappings belong to the user; only seed an unmapped action.
if not any(m.get_editor_property('action') == action for m in context.get_editor_property('mappings')):
    for name in ('R', 'Gamepad_FaceButton_Top'):
        key = unreal.Key()
        key.import_text(name)
        context.map_key(action, key)
assert library.save_loaded_asset(action)
# MapKey does not mark an already loaded IMC package dirty in a commandlet.
assert library.save_loaded_asset(context, False)
unreal.log('SETTINGS_RESET_PAGE_INPUT_SUCCESS')
