"""Import the reviewed utility jeep and replace the demo's cube traffic."""
import unreal
import json
import math
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
root=Path(unreal.Paths.project_dir());folder='/Game/LivingWorld/Models/Jeep'
tools=unreal.AssetToolsHelpers.get_asset_tools()
task=unreal.AssetImportTask();task.filename=str(root/'Art/LivingWorld/Prepared/Ground/Jeep.fbx')
task.destination_path=folder;task.destination_name='SK_Jeep';task.automated=task.save=task.replace_existing=True
options=unreal.FbxImportUI();options.import_as_skeletal=True;options.import_animations=True
options.automated_import_should_detect_type=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.import_materials=options.import_textures=True;task.options=options;tools.import_asset_tasks([task])
assets=[a.get_asset() for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder)]
mesh=next(a for a in assets if isinstance(a,unreal.SkeletalMesh));clip=next(a for a in assets if isinstance(a,unreal.AnimSequence))
editor=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
mesh.set_editor_property('lod_settings',unreal.load_asset('/Game/LivingWorld/Models/Flight/DA_FlightLODSettings'))
assert editor.regenerate_lod(mesh,4,False,False)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
profile=unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
if not profile:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.LivingAssetProfile)
    profile=tools.create_asset('DA_Jeep','/Game/LivingWorld/Profiles',unreal.LivingAssetProfile,factory)
manifest=json.loads((root/'Art/LivingWorld/Prepared/Ground/Jeep.json').read_text())
wheels=[]
for name in ('LF','RF','LR','RR'):
    wheel=unreal.LivingWheelDefinition()
    wheel.bone='wheel_'+name;wheel.radius_cm=manifest['wheel_radius_m']*100;wheel.steers=name.endswith('F')
    wheels.append(wheel)
profile.modify()
for key,value in {'kind':unreal.LivingKind.CAR,'skeletal_mesh':mesh,'cruise_animation':clip,'approved':True,
    'visual_rotation':unreal.Rotator(yaw=-90),'visual_scale':unreal.Vector(1,1,1),
    'visual_offset':unreal.Vector(0,0,-110),'ground_clearance_cm':110.,'collision_radius_cm':105.,
    'speed_meters_per_second':6.,'cruise_cycle_meters':math.tau*manifest['wheel_radius_m'],
    'wheels':wheels,'suspension_travel_cm':20.,'max_wheel_steering_degrees':35.,
    'source':'Tripo existing user task 27449fe4-f4cd-441b-8edc-ebc01c6b3137',
    'license':'User-generated Tripo asset; no additional generation credits',
    'scale_evidence':'Generic utility vehicle, representative overall length 4.1 m; not a verified military subtype'}.items():
    profile.set_editor_property(key,value)
unreal.EditorAssetLibrary.save_loaded_asset(profile,False)
proxy=unreal.load_asset('/Game/LivingWorld/Demo/DA_TestCar')
if proxy:proxy.set_editor_property('approved',False);unreal.EditorAssetLibrary.save_loaded_asset(proxy)
report={'profile':profile.get_path_name(),'mesh':mesh.get_path_name(),'clip':clip.get_path_name(),
    'vertices_per_lod':[editor.get_num_verts(mesh,i) for i in range(4)],'wheel_cycle_m':profile.cruise_cycle_meters}
(root/'Saved/LivingWorld/jeep-import.json').write_text(json.dumps(report,indent=2))
print('JEEP_IMPORTED',report)
