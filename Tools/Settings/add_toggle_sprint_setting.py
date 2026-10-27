"""Add toggle sprint without replacing the project's edited settings definitions."""
import json
from pathlib import Path
import unreal

asset = unreal.EditorAssetLibrary.load_asset('/Game/Settings/DA_Settings')
assert asset
draft = unreal.new_object(unreal.TASettingsDefinitionAsset)
draft.call_method('PopulateDefaultDefinitions')
setting_id = 'Game.ToggleSprint'
definition = next(d for d in draft.get_editor_property('definitions')
                  if str(d.get_editor_property('setting_id')) == setting_id)
definitions = list(asset.get_editor_property('definitions'))
if not any(str(d.get_editor_property('setting_id')) == setting_id for d in definitions):
    index = next((i for i, d in enumerate(definitions)
                  if not str(d.get_editor_property('setting_id')).startswith('Game.')), len(definitions))
    definitions.insert(index, definition)
    asset.set_editor_property('definitions', definitions)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
assert sum(str(d.get_editor_property('setting_id')) == setting_id for d in definitions) == 1
for language in ('en', 'zh-CN', 'zh-TW'):
    texts = json.loads((Path(unreal.Paths.project_content_dir()) / 'Localization' / (language + '.json')).read_text(encoding='utf-8-sig'))
    for field in ('name_text_id', 'description_text_id'):
        assert definition.get_editor_property(field) in texts
unreal.log('TOGGLE_SPRINT_SETTING_SUCCESS')
