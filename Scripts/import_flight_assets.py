"""Import the reviewed airborne family, build LODs, and configure runtime profiles.

Run in the editor with PIE stopped. Originals and former draft packages survive.
"""
import unreal
import json
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
project=Path(unreal.Paths.project_dir())
tools=unreal.AssetToolsHelpers.get_asset_tools()
registry=unreal.AssetRegistryHelpers.get_asset_registry()
mesh_editor=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
folder='/Game/LivingWorld/Models/Flight'
settings=unreal.load_asset(folder+'/DA_FlightLODSettings')
if not settings:
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.SkeletalMeshLODSettings)
    settings=tools.create_asset('DA_FlightLODSettings',folder,unreal.SkeletalMeshLODSettings,factory)
groups=[]
for fraction,screen in [(1.,1.),(.5,.3),(.2,.12),(.06,.035)]:
    group=unreal.SkeletalMeshLODGroupSettings()
    reduction=group.get_editor_property('reduction_settings')
    reduction.set_editor_property('num_of_triangles_percentage',fraction)
    reduction.set_editor_property('base_lod',0)
    reduction.set_editor_property('max_bones_per_vertex',4)
    group.set_editor_property('reduction_settings',reduction)
    group.set_editor_property('screen_size',unreal.PerPlatformFloat(default=screen))
    groups.append(group)
settings.set_editor_property('lod_groups',groups)
unreal.EditorAssetLibrary.save_loaded_asset(settings)

# Speeds and flight lanes are scenery tuning, never a real vehicle dynamics model.
specs={
    'Mavic3':(unreal.LivingKind.DRONE,7.,25.,20.,.20,0.),
    'Orqa':(unreal.LivingKind.DRONE,12.,35.,18.,.28,0.),
    'F4':(unreal.LivingKind.PLANE,80.,160.,550.,.85,180.),
    'UH1':(unreal.LivingKind.HELICOPTER,30.,75.,550.,.55,180.),
    'AH1W':(unreal.LivingKind.HELICOPTER,38.,105.,550.,.65,180.),
    'DetailedPigeon':(unreal.LivingKind.BIRD,10.,22.,20.,.25,-90.),
    'Gull':(unreal.LivingKind.BIRD,12.,38.,35.,.25,-90.),
    'Crow':(unreal.LivingKind.BIRD,11.,28.,25.,.25,-90.),
}
requested=globals().get('living_flight_import_names')
if requested:
    assert set(requested)<=set(specs),requested
    specs={name:spec for name,spec in specs.items() if name in requested}
report=[]
for name,(kind,speed,altitude,radius,lane,yaw) in specs.items():
    destination=folder+'/'+name
    source=project/'Art/LivingWorld/Prepared'/('Orqa.fbx' if name=='Orqa' else 'Flight/'+name+'.fbx')
    task=unreal.AssetImportTask(); task.filename=str(source); task.destination_path=destination
    task.destination_name='SK_'+name; task.automated=True; task.save=True; task.replace_existing=True
    options=unreal.FbxImportUI(); options.import_as_skeletal=True
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type=False
    options.import_animations=True; options.import_materials=True; options.import_textures=True
    options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose',False)
    task.options=options; tools.import_asset_tasks([task])
    assets=[a.get_asset() for a in registry.get_assets_by_path(destination)]
    mesh=next(a for a in assets if isinstance(a,unreal.SkeletalMesh))
    clips=[a for a in assets if isinstance(a,unreal.AnimSequence)]
    assert clips,name+' missing animation'
    source_vertices=mesh_editor.get_num_verts(mesh,0)
    mesh.set_editor_property('lod_settings',settings)
    assert mesh_editor.regenerate_lod(mesh,4,False,False),name
    counts=[mesh_editor.get_num_verts(mesh,i) for i in range(mesh_editor.get_lod_count(mesh))]
    assert counts[0]==source_vertices and len(counts)==4 and counts[-1]<counts[0],counts
    # Thin feathers and rotor blades need both sides visible from sensor cameras.
    for asset in assets:
        if isinstance(asset,unreal.Material):
            asset.set_editor_property('two_sided',True)
            unreal.MaterialEditingLibrary.recompile_material(asset)
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    path='/Game/LivingWorld/Profiles/DA_'+name
    profile=unreal.load_asset(path)
    if not profile:
        factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.LivingAssetProfile)
        profile=tools.create_asset('DA_'+name,'/Game/LivingWorld/Profiles',unreal.LivingAssetProfile,factory)
    bird=kind==unreal.LivingKind.BIRD
    cruise=next((a for a in clips if 'Flapping' in a.get_name()),clips[0])
    values={'kind':kind,'skeletal_mesh':mesh,'static_mesh':None,'cruise_animation':cruise,
        'visual_scale':unreal.Vector(1,1,1),'visual_rotation':unreal.Rotator(yaw=yaw),
        'speed_meters_per_second':speed,'altitude_meters':altitude,'collision_radius_cm':radius,
        'flight_radius_fraction':lane,'max_bank_degrees':35. if kind==unreal.LivingKind.PLANE else 20.,
        'turn_rate_degrees':25. if kind==unreal.LivingKind.PLANE else 55.,'allow_perching':False,
        'source':str(source),'approved':False,
        'license':'AI-generated Tripo model; source and credit ledger in Art/LivingWorld' if bird else 'User-supplied private project asset; redistribution rights unverified',
        'scale_evidence':('Representative wingspan: 1.40 m gull / 0.68 m pigeon / 0.95 m crow; artistic anatomy' if bird else
            'Source metre geometry; inspected against supplied source. Orqa 0.127 m propeller scale. Not engineering measurement data')}
    if bird:values['glide_animation']=next(a for a in clips if 'Gliding' in a.get_name())
    for key,value in values.items():profile.set_editor_property(key,value)
    unreal.EditorAssetLibrary.save_loaded_asset(profile)
    report.append({'name':name,'mesh':mesh.get_path_name(),'vertices_per_lod':counts,
        'clips':[a.get_path_name() for a in clips],'bounds':str(mesh.get_bounds()),'profile':path})
report_path=project/'Saved/LivingWorld/flight-import.json'
if requested and report_path.exists():
    report=[entry for entry in json.loads(report_path.read_text()) if entry['name'] not in requested]+report
report_path.write_text(json.dumps(report,indent=2))
print('FLIGHT_IMPORT_READY',[(x['name'],x['vertices_per_lod']) for x in report])
