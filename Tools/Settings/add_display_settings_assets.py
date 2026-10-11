"""Add four display choices to DA_Settings without replacing edited definitions."""
import json
from pathlib import Path
import unreal

library = unreal.EditorAssetLibrary
asset = library.load_asset('/Game/Setting/DA_Settings')
assert asset
draft = unreal.new_object(unreal.TASettingsDefinitionAsset)
draft.call_method('PopulateDefaultDefinitions')
ids = ['Display.Monitor', 'Display.WindowMode', 'Display.AspectRatio', 'Display.Resolution']
defaults = {str(d.get_editor_property('setting_id')): d for d in draft.get_editor_property('definitions')}
definitions = list(asset.get_editor_property('definitions'))
existing = {str(d.get_editor_property('setting_id')) for d in definitions}
pages = asset.get_editor_property('page_definition_asset')
assert pages
page = next((p for p in pages.get_editor_property('pages') if str(p.get_editor_property('page_id')) == 'Settings.Page.Display'), None)
assert page, 'Display page is missing'
sections = {str(s.get_editor_property('section_id')) for s in page.get_editor_property('sections')}
assert 'Settings.Section.Display.Video' in sections, 'Display.Video section is missing'
missing = [defaults[name] for name in ids if name not in existing]
index = next((i for i, d in enumerate(definitions) if str(d.get_editor_property('setting_id')).startswith('Display.')), len(definitions))
definitions[index:index] = missing
asset.set_editor_property('definitions', definitions)
assert library.save_loaded_asset(asset, False)
translations = [json.loads((Path(unreal.Paths.project_content_dir()) / 'Localization' / (lang + '.json')).read_text(encoding='utf-8-sig')) for lang in ('zh-CN', 'zh-TW', 'en')]
for d in definitions:
    if str(d.get_editor_property('setting_id')) in ids:
        assert d.get_editor_property('type') == unreal.TASettingType.CHOICE
        for field in ('name_text_id', 'description_text_id'):
            assert all(d.get_editor_property(field) in t for t in translations)
unreal.log('DISPLAY_SETTINGS_ASSETS_SUCCESS: added ' + ', '.join(str(d.get_editor_property('setting_id')) for d in missing))
