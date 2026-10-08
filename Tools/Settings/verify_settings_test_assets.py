"""Read the saved settings test assets in a fresh Unreal process."""
import unreal

library = unreal.EditorAssetLibrary
asset = library.load_asset("/Game/Setting/DA_Settings")
assert asset
definitions = asset.get_editor_property("definitions")
assert len(definitions) == 12
assert all(d.get_editor_property("type") != unreal.TASettingType.SUBMENU for d in definitions)
assert all(not d.get_editor_property("submenu_only") for d in definitions)
for d in definitions:
    assert len(d.get_editor_property("description_blocks")) >= 1
unreal.log("SETTINGS_SAVED_CATALOG_VERIFIED: 12 settings, no submenus")

menu_class = library.load_blueprint_class("/Game/UI/Setting/WBP_SettingsMenu")
assert menu_class
menu = unreal.get_default_object(menu_class)
for name in ("page_widget_class", "section_widget_class", "description_block_widget_class", "action_prompt_widget_class"):
    unreal.log("SETTINGS_MENU_CONFIG: " + name + "=" + str(menu.get_editor_property(name)))
unreal.log("SETTINGS_MENU_CONFIG: row_widget_classes=" + str(menu.get_editor_property("row_widget_classes")))
assert menu.get_editor_property("default_menu_definition") is None

pc_class = library.load_blueprint_class("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController")
if pc_class:
    pc = unreal.get_default_object(pc_class)
    unreal.log("SETTINGS_PC_CONFIG: " + str(pc.get_editor_property("settings_menu_widget_class")))
    assert pc.get_editor_property("settings_menu_widget_class") == menu_class
