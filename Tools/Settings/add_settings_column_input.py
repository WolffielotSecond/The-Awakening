"""Add left-stick column navigation without replacing existing settings mappings."""
import unreal

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
context = library.load_asset('/Game/Input/IMC_Settings')
assert context
for suffix, key_name in (('FocusCategories', 'Gamepad_LeftStick_Left'),
                         ('FocusDetails', 'Gamepad_LeftStick_Right')):
    name = 'IA_Settings' + suffix
    folder = '/Game/Input/Actions/Settings'
    path = folder + '/' + name
    action = library.load_asset(path) if library.does_asset_exist(path) else None
    if not action:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.InputAction)
        action = tools.create_asset(name, folder, unreal.InputAction, factory)
    assert action
    action.set_editor_property('trigger_when_paused', True)
    if not any(m.get_editor_property('action') == action for m in context.get_editor_property('mappings')):
        key = unreal.Key()
        key.import_text(key_name)
        context.map_key(action, key)
    assert library.save_loaded_asset(action)
assert library.save_loaded_asset(context, False)
unreal.log('SETTINGS_COLUMN_INPUT_SUCCESS')
