"""Run with UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this path>."""
import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_saved_dir())/'DiaryImport'
OUT.mkdir(parents=True,exist_ok=True)
SOURCE=Path(r'C:\Users\wolfs\Documents\ChatGPT\学习\Awakening_Journal\The_Awakening_Journal_Delivery')
DEST='/Game/Models/Diary'
if unreal.EditorAssetLibrary.list_assets(DEST,recursive=True,include_folder=False):
    raise RuntimeError('Diary folder already contains assets; refusing to overwrite.')
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
tools=unreal.AssetToolsHelpers.get_asset_tools()
report={}
def imp(name,skeleton=None,animation=False):
    ui=unreal.FbxImportUI()
    ui.set_editor_property('automated_import_should_detect_type',False)
    ui.set_editor_property('import_mesh',not animation)
    ui.set_editor_property('import_as_skeletal',True)
    ui.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('import_animations',animation)
    ui.set_editor_property('import_materials',not animation)
    ui.set_editor_property('import_textures',False)
    ui.set_editor_property('create_physics_asset',False)
    if skeleton:ui.set_editor_property('skeleton',skeleton)
    ui.skeletal_mesh_import_data.set_editor_property('import_uniform_scale',1.0)
    ui.skeletal_mesh_import_data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    ui.anim_sequence_import_data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task=unreal.AssetImportTask();task.filename=str(SOURCE/'FBX'/(name+'.fbx'));task.destination_path=DEST
    task.destination_name=name;task.automated=True;task.replace_existing=False;task.save=True;task.options=ui
    tools.import_asset_tasks([task])
    objects=task.get_objects()
    assert objects,'No imported assets: '+name
    report[name]=[{'path':o.get_path_name(),'class':o.get_class().get_name()} for o in objects]
    for o in objects:
        if isinstance(o,unreal.SkeletalMesh):
            c=unreal.SkeletalMeshComponent();c.set_skeletal_mesh_asset(o)
            names=[str(c.get_bone_name(i)) for i in range(c.get_num_bones())]
            report[name].append({'bone_names':names,'skeleton':o.skeleton.get_path_name()})
            assert 'Root' in names
            if name!='SK_Collectible_A4':assert 'Cover_Front' in names and 'Cover_Back' in names
            if skeleton:assert o.skeleton==skeleton,'Skeleton mismatch: '+name
    return objects
try:
    b=next(o for o in imp('SK_Journal') if isinstance(o,unreal.SkeletalMesh))
    d=next(o for o in imp('SK_Collectible_A4') if isinstance(o,unreal.SkeletalMesh))
    assert b.skeleton != d.skeleton, 'Document must use its own skeleton'
    imp('SK_Journal_Body',b.skeleton);imp('SK_TurningPage',b.skeleton)
    for n in ['Book_Open','Book_Close','Page_Turn_Forward','Page_Turn_Backward','Document_Extract','Document_Insert']:
        objs=imp(n,d.skeleton if n.startswith('Document') else b.skeleton,True)
        a=next(o for o in objs if isinstance(o,unreal.AnimSequence))
        assert a.get_play_length()>.9,n
        assert a.get_editor_property('skeleton') == (d.skeleton if n.startswith('Document') else b.skeleton),n
        report[n].append({'skeleton':a.get_editor_property('skeleton').get_path_name()})
        report[n].append({'duration_seconds':a.get_play_length()})
    assert unreal.EditorAssetLibrary.save_directory(DEST,only_if_is_dirty=False,recursive=True)
    report['status']='PASS'
except Exception as e:
    report['status']='FAIL';report['error']=str(e)
    raise
finally:
    (OUT/'ue_validation.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('JOURNAL_UE_VALIDATION '+report['status'])



