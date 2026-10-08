"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Creates the initial non-submenu test catalog. Existing settings are never replaced.
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary


def create_data_asset(name, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, "/Game/Setting", cls, factory)


def text_block(text_id):
    block = unreal.TASettingDescriptionBlock()
    block.set_editor_property("type", unreal.TASettingDescriptionBlockType.TEXT)
    block.set_editor_property("text_id", text_id)
    return block


def image_block(texture):
    block = unreal.TASettingDescriptionBlock()
    block.set_editor_property("type", unreal.TASettingDescriptionBlockType.IMAGE)
    block.set_editor_property("image", texture)
    return block


settings_path = "/Game/Setting/DA_Settings"
if library.does_asset_exist(settings_path):
    raise RuntimeError("DA_Settings already exists; refusing to replace editable definitions.")

pages = library.load_asset("/Game/Setting/DA_SettingsPages")
if not pages:
    pages = create_data_asset("DA_SettingsPages", unreal.TASettingsPageDefinitionAsset)
    pages.call_method("PopulateDefaultPages")
    library.save_loaded_asset(pages)

allowed = {
    "Game.Language", "Game.DialogueTextSpeed", "Display.VSync",
    "Display.FrameRateLimit", "Display.AdvancedData",
    "MouseKeyboard.CameraSensitivity", "MouseKeyboard.InvertCameraX",
    "MouseKeyboard.InvertCameraY", "Controller.CameraSensitivity",
    "Controller.InvertCameraX", "Controller.InvertCameraY",
    "Controller.MenuCursorSpeed",
}
# Generate in memory first, so an invalid page catalog cannot leave a partial saved asset.
draft = unreal.new_object(unreal.TASettingsDefinitionAsset)
draft.call_method("PopulateDefaultDefinitions")
definitions = [d for d in draft.get_editor_property("definitions")
               if str(d.get_editor_property("setting_id")) in allowed]
assert len(definitions) == len(allowed)
page_lookup = {
    str(p.get_editor_property("page_id")): {
        str(s.get_editor_property("section_id")) for s in p.get_editor_property("sections")
    } for p in pages.get_editor_property("pages")
}
for definition in definitions:
    for loc in definition.get_editor_property("locations").get_editor_property("items"):
        page_id = str(loc.get_editor_property("page_id"))
        section_id = str(loc.get_editor_property("section_id"))
        if section_id not in page_lookup.get(page_id, set()):
            raise RuntimeError(f"DA_SettingsPages is missing {page_id}/{section_id}.")

tasks = []
for name in ("T_SettingsWide", "T_SettingsTall"):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(ROOT / "Content/Setting/TestImages" / (name + ".png")))
    task.set_editor_property("destination_path", "/Game/Setting/TestImages")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    if not library.does_asset_exist("/Game/Setting/TestImages/" + name):
        tasks.append(task)
if tasks:
    tools.import_asset_tasks(tasks)
wide = library.load_asset("/Game/Setting/TestImages/T_SettingsWide")
tall = library.load_asset("/Game/Setting/TestImages/T_SettingsTall")
assert wide and tall
for texture in (wide, tall):
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    library.save_loaded_asset(texture)

for definition in definitions:
    setting_id = str(definition.get_editor_property("setting_id"))
    blocks = [text_block(definition.get_editor_property("description_text_id"))]
    if setting_id == "MouseKeyboard.CameraSensitivity":
        blocks += [image_block(wide), text_block("Settings.Test.ImageLayout.Description"), image_block(tall)]
    elif setting_id == "Controller.CameraSensitivity":
        blocks += [image_block(tall), text_block("Settings.Test.ImageLayout.Description")]
    definition.set_editor_property("description_blocks", blocks)

asset = create_data_asset("DA_Settings", unreal.TASettingsDefinitionAsset)
asset.set_editor_property("page_definition_asset", pages)
asset.set_editor_property("definitions", definitions)
if not library.save_loaded_asset(asset):
    raise RuntimeError("Could not save DA_Settings.")

# Verify the on-disk catalog, localization and image references.
report = []
localization = {
    lang: json.loads((ROOT / "Content/Localization" / (lang + ".json")).read_text(encoding="utf-8-sig"))
    for lang in ("zh-CN", "zh-TW", "en")
}
for definition in asset.get_editor_property("definitions"):
    for key in (definition.get_editor_property("name_text_id"), definition.get_editor_property("description_text_id")):
        assert all(key in translations for translations in localization.values()), key
    for block in definition.get_editor_property("description_blocks"):
        if block.get_editor_property("type") == unreal.TASettingDescriptionBlockType.TEXT:
            key = block.get_editor_property("text_id")
            assert all(key in translations for translations in localization.values()), key
        else:
            assert block.get_editor_property("image")
    report.append(str(definition.get_editor_property("setting_id")))
assert set(report) == allowed
assert all(d.get_editor_property("type") != unreal.TASettingType.SUBMENU for d in asset.get_editor_property("definitions"))
unreal.log("SETTINGS_TEST_ASSETS_SUCCESS: " + ", ".join(report))
