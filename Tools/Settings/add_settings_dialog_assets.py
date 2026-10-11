"""Incrementally add Apply and reusable dialog templates; preserve authored assets/mappings."""
import unreal

lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def data(name, folder, cls):
    path = folder + '/' + name
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return tools.create_asset(name, folder, cls, factory)


def seed(context, name, keys, folder):
    action = data(name, folder, unreal.InputAction)
    assert action
    action.set_editor_property('trigger_when_paused', True)
    if not any(m.get_editor_property('action') == action for m in context.get_editor_property('mappings')):
        for text in keys:
            key = unreal.Key()
            key.import_text(text)
            context.map_key(action, key)
    assert lib.save_loaded_asset(action)


settings = lib.load_asset('/Game/Input/IMC_Settings')
assert settings
seed(settings, 'IA_SettingsApply', ('Enter', 'Gamepad_Special_Right'), '/Game/Input/Actions/Settings')
assert lib.save_loaded_asset(settings, False)
dialog = data('IMC_CommonDialog', '/Game/Input', unreal.InputMappingContext)
assert dialog
for slot, keys in enumerate((('Enter', 'Gamepad_FaceButton_Bottom'),
                             ('Delete', 'Gamepad_FaceButton_Right'),
                             ('Escape', 'Gamepad_FaceButton_Left'),
                             ('SpaceBar', 'Gamepad_FaceButton_Top')), 1):
    seed(dialog, 'IA_DialogOption' + str(slot), keys, '/Game/Input/Actions/Dialog')
assert lib.save_loaded_asset(dialog, False)

for name, parent in (('WBP_CommonDialogButton', unreal.TACommonDialogButtonWidget),
                     ('WBP_CommonDialog', unreal.TACommonDialogWidget)):
    path = '/Game/UI/Common/' + name
    if not lib.does_asset_exist(path):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        asset = tools.create_asset(name, '/Game/UI/Common', unreal.WidgetBlueprint, factory)
        assert asset and lib.save_loaded_asset(asset)
unreal.log('SETTINGS_DIALOG_ASSETS_SUCCESS')
