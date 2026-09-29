"""Build reversible runtime LODs while retaining imported source LOD0 and bones."""
import unreal
import json
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
tools=unreal.AssetToolsHelpers.get_asset_tools()
subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
path='/Game/LivingWorld/Models/DA_HumanLODSettings'
settings=unreal.load_asset(path)
if not settings:
    factory=unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class',unreal.SkeletalMeshLODSettings)
    settings=tools.create_asset('DA_HumanLODSettings','/Game/LivingWorld/Models',unreal.SkeletalMeshLODSettings,factory)
groups=[]
for fraction,screen in [(1.,1.),(.5,.25),(.2,.1),(.08,.035)]:
    group=unreal.SkeletalMeshLODGroupSettings()
    reduction=group.get_editor_property('reduction_settings')
    reduction.set_editor_property('num_of_triangles_percentage',fraction)
    reduction.set_editor_property('base_lod',0)
    reduction.set_editor_property('max_bones_per_vertex',4)
    reduction.set_editor_property('merge_coincident_vert_bones',True)
    group.set_editor_property('reduction_settings',reduction)
    group.set_editor_property('screen_size',unreal.PerPlatformFloat(default=screen))
    groups.append(group)
settings.set_editor_property('lod_groups',groups)
unreal.EditorAssetLibrary.save_loaded_asset(settings)
report=[]
for kind in ('Civilian','Soldier'):
    mesh=unreal.load_asset('/Game/LivingWorld/Models/'+kind+'/SK_'+kind)
    source_vertices=subsystem.get_num_verts(mesh,0)
    mesh.set_editor_property('lod_settings',settings)
    assert subsystem.regenerate_lod(mesh,4,False,False),kind
    counts=[subsystem.get_num_verts(mesh,i) for i in range(subsystem.get_lod_count(mesh))]
    assert len(counts)==4 and counts[0]==source_vertices and all(0<counts[i]<counts[i-1] for i in range(1,4)),counts
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    report.append({'mesh':mesh.get_path_name(),'vertices_per_lod':counts,'screen_sizes':[1.,.25,.1,.035],
                   'source_lod_preserved':True})
(Path(unreal.Paths.project_saved_dir())/'LivingWorld/human-lods.json').write_text(json.dumps(report,indent=2))
print('HUMAN_LODS',report)
