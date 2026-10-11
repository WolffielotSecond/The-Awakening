"""Verify saved IMC mappings and template connections without altering assets."""
import unreal
library = unreal.EditorAssetLibrary
context = library.load_asset("/Game/Input/IMC_Settings")
assert context
mappings = context.get_editor_property("mappings")
names = {str(m.get_editor_property("action").get_name()) for m in mappings}
assert "IA_SettingsResetPage" in names
assert "IA_SettingsApply" in names
for m in mappings:
    action = m.get_editor_property("action")
    key = m.get_editor_property("key").export_text()
    assert action.get_editor_property("trigger_when_paused")
    if action.get_name() in ("IA_SettingsPreviousItem", "IA_SettingsNextItem", "IA_SettingsConfirm", "IA_SettingsScrollUp", "IA_SettingsScrollDown", "IA_SettingsPointerClick"):
        assert "Gamepad_" in key, (action.get_name(), key)
pc_class = library.load_blueprint_class("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController")
pc = unreal.get_default_object(pc_class)
assert pc.get_editor_property("settings_input_mapping_context") == context
unreal.log("SETTINGS_IMC_VERIFIED: paused settings actions including ResetPage; keyboard has no item navigation or confirm")

menu_class = library.load_blueprint_class("/Game/UI/Setting/WBP_SettingsMenu")
menu = unreal.get_default_object(menu_class)
assert len(menu.get_editor_property("row_widget_classes")) == 4
assert menu.get_editor_property("description_block_widget_class")
unreal.log("SETTINGS_TEMPLATE_REFERENCES_VERIFIED")

dialog = library.load_asset("/Game/Input/IMC_CommonDialog")
assert dialog
dialog_names = set()
for mapping in dialog.get_editor_property("mappings"):
    action = mapping.get_editor_property("action")
    assert action and action.get_editor_property("trigger_when_paused")
    dialog_names.add(action.get_name())
assert dialog_names == {"IA_DialogOption" + str(i) for i in range(1, 5)}
for path in ("/Game/UI/Common/WBP_CommonDialog", "/Game/UI/Common/WBP_CommonDialogButton"):
    assert library.load_blueprint_class(path)
unreal.log("SETTINGS_DIALOG_ASSETS_VERIFIED: Apply, four paused dialog actions, and both WBP templates")
