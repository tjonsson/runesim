"""Import the September 30 asset batch (editor, PIE stopped) and create approved Living World profiles.

Inputs (Blender outputs): Prepared/Ground/{Humvee,Sedan,Ariete}.fbx, Prepared/Flight/TB2.fbx and
Prepared/CivilianMan/*.fbx. Each profile starts as a copy of the closest reviewed profile (jeep,
Global Hawk, civilian), so sounds, footsteps, feet and reaction tuning carry over; only mesh,
animation, size and speed change. Writes review renders to Saved/LivingWorld/<Name>-unreal.png.
"""
import json
import math
from pathlib import Path
import unreal

assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
root = Path(unreal.Paths.project_dir())
art = root / 'Art/LivingWorld/Prepared'
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
skeletal = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
report = {}


def import_fbx(path, folder, name, skeletal_mesh=True, animations=False, skeleton=None, mesh=True):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = folder
    task.destination_name = name
    task.automated = task.save = task.replace_existing = True
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_mesh = mesh
    options.import_as_skeletal = skeletal_mesh
    options.import_animations = animations
    options.import_materials = options.import_textures = mesh
    if skeletal_mesh:
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if mesh else unreal.FBXImportType.FBXIT_ANIMATION
        options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
    else:
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.set_editor_property('combine_meshes', True)
        options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
    if skeleton:
        options.skeleton = skeleton
    task.options = options
    tools.import_asset_tasks([task])
    return list(task.get_objects())


def profile_from(template, name, values):
    path = '/Game/LivingWorld/Profiles/' + name
    profile = unreal.load_asset(path)
    if not profile:
        profile = lib.duplicate_asset('/Game/LivingWorld/Profiles/' + template, path)
    profile.modify()
    for key, value in values.items():
        profile.set_editor_property(key, value)
    # Cleared template references (e.g. the jeep's mesh on the tank) must really be empty.
    for key, value in values.items():
        if value is None and profile.get_editor_property(key) is not None:
            profile.set_editor_property(key, None)
        assert value is not None or profile.get_editor_property(key) is None, (name, key)
    lib.save_loaded_asset(profile, False)
    return profile


def wheels(radius_m):
    result = []
    for name in ('LF', 'RF', 'LR', 'RR'):
        wheel = unreal.LivingWheelDefinition()
        wheel.bone = 'wheel_' + name
        wheel.radius_cm = radius_m * 100
        wheel.steers = name.endswith('F')
        result.append(wheel)
    return result


lod_settings = unreal.load_asset('/Game/LivingWorld/Models/Flight/DA_FlightLODSettings')

# The existing generated jeep is a military utility vehicle.
jeep = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
jeep.modify(); jeep.set_editor_property('military', True); lib.save_loaded_asset(jeep, False)

# The spawn test places a sphere of the footprint radius at clearance height, so clearance must exceed it.
for name, military, speed, radius_cm, clearance in (('Humvee', True, 7., 115., 125.), ('Sedan', False, 9., 110., 115.)):
    manifest = json.loads((art / f'Ground/{name}.json').read_text())
    folder = '/Game/LivingWorld/Models/' + name
    objects = import_fbx(art / f'Ground/{name}.fbx', folder, 'SK_' + name, animations=True)
    assets = [a.get_asset() for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder)]
    mesh = next(a for a in assets if isinstance(a, unreal.SkeletalMesh))
    clip = next(a for a in assets if isinstance(a, unreal.AnimSequence))
    mesh.set_editor_property('lod_settings', lod_settings)
    assert skeletal.regenerate_lod(mesh, 4, False, False), name
    lib.save_loaded_asset(mesh)
    profile_from('DA_Jeep', 'DA_' + name, {
        'skeletal_mesh': mesh, 'cruise_animation': clip, 'approved': True, 'military': military,
        'visual_rotation': unreal.Rotator(yaw=-90), 'visual_offset': unreal.Vector(0, 0, -clearance), 'ground_clearance_cm': clearance,
        'collision_radius_cm': radius_cm, 'speed_meters_per_second': speed,
        'cruise_cycle_meters': math.tau * manifest['wheel_radius_m'], 'wheels': wheels(manifest['wheel_radius_m']),
        'source': f"Tripo H3.1 text-to-3D task {manifest['source_task']}",
        'license': 'Account-generated Tripo asset (user-authorized credits)',
        'scale_evidence': f"{manifest['scope']}; normalized length {manifest['length_m']} m, wheelbase {manifest['wheelbase_m']:.2f} m"})
    report[name] = {'mesh': mesh.get_path_name(), 'vertices_per_lod': [skeletal.get_num_verts(mesh, i) for i in range(4)]}

# Tracked tank: rigid mesh (tracks and turret are not articulated). The footprint radius must fit the
# reviewed 2.5 m dirt road, so the 3.6 m hull overhangs the verges; the shoulder lane is 3 m off-centre.
# Collision radius is the route footprint; the HMMWV (2.2 m body) uses 115 cm for the same reason.
ariete = json.loads((art / 'Ground/Ariete.json').read_text())
folder = '/Game/LivingWorld/Models/Ariete'
import_fbx(art / 'Ground/Ariete.fbx', folder, 'SM_Ariete', skeletal_mesh=False)
tank = unreal.load_asset(folder + '/SM_Ariete')
static = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
reduction = unreal.EditorScriptingMeshReductionOptions()
reduction.reduction_settings = [unreal.EditorScriptingMeshReductionSettings(p, s) for p, s in ((1., 1.), (.4, .3), (.15, .12), (.05, .035))]
reduction.auto_compute_lod_screen_size = False
static.set_lods_with_notification(tank, reduction, False)
lib.save_loaded_asset(tank, False)
profile_from('DA_Jeep', 'DA_Ariete', {
    'skeletal_mesh': None, 'static_mesh': tank, 'cruise_animation': None, 'wheels': [], 'approved': True, 'military': True,
    'visual_rotation': unreal.Rotator(yaw=-90), 'visual_offset': unreal.Vector(0, 0, -130), 'ground_clearance_cm': 130.,
    'collision_radius_cm': 120., 'speed_meters_per_second': 5., 'target_health': 400., 'cruise_cycle_meters': 0.,
    'source': ariete['source'] + ' (author ' + ariete['author'] + ')', 'license': ariete['license'],
    'scale_evidence': f"Published C1 Ariete hull length 7.59 m; overall {ariete['overall_length_m']} m, width {ariete['width_m']} m"})
report['Ariete'] = {'mesh': tank.get_path_name(), 'lods': static.get_lod_count(tank)}

# TB2-style drone: skeletal for the spinning pusher propeller.
tb2 = json.loads((art / 'Flight/TB2.json').read_text())
folder = '/Game/LivingWorld/Models/Flight/TB2'
import_fbx(art / 'Flight/TB2.fbx', folder, 'SK_TB2', animations=True)
assets = [a.get_asset() for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder)]
mesh = next(a for a in assets if isinstance(a, unreal.SkeletalMesh))
clip = next(a for a in assets if isinstance(a, unreal.AnimSequence))
mesh.set_editor_property('lod_settings', lod_settings)
assert skeletal.regenerate_lod(mesh, 4, False, False)
lib.save_loaded_asset(mesh)
profile_from('DA_GlobalHawk', 'DA_TB2', {
    'static_mesh': None, 'skeletal_mesh': mesh, 'cruise_animation': clip, 'approved': True,
    'visual_rotation': unreal.Rotator(yaw=-90), 'visual_scale': unreal.Vector(1, 1, 1), 'visual_offset': unreal.Vector(),
    # Scenery tuning for a MALE drone in the local airspace, not Bayraktar performance data.
    'speed_meters_per_second': 36., 'altitude_meters': 450., 'collision_radius_cm': 600., 'turn_rate_degrees': 14.,
    'max_bank_degrees': 25., 'loop_sound': unreal.load_asset('/Game/LivingWorld/Audio/Ambient/utility_engine_loop'),
    'source': f"Tripo H3.1 image-to-3D task {tb2['source_task']} from {tb2['reference']}",
    'license': 'Account-generated Tripo asset (user-authorized credits)', 'scale_evidence': tb2['scope']})
report['TB2'] = {'mesh': mesh.get_path_name(), 'vertices_per_lod': [skeletal.get_num_verts(mesh, i) for i in range(4)]}

# Second civilian: same pipeline as the original humans, own skeleton and blend space.
manifest = json.loads((art / 'CivilianMan/manifest.json').read_text())
folder = '/Game/LivingWorld/Models/CivilianMan'
clips = manifest['clips']
objects = import_fbx(art / f"CivilianMan/{clips[0]['name']}.fbx", folder, 'SK_CivilianMan')
mesh = next(o for o in objects if isinstance(o, unreal.SkeletalMesh))
skeleton = mesh.get_editor_property('skeleton')
animations = {}
for clip in clips:
    imported = import_fbx(art / f"CivilianMan/{clip['name']}.fbx", folder, clip['name'], animations=True, skeleton=skeleton, mesh=False)
    animations[clip['name'].split('_')[-1]] = next(o for o in imported if isinstance(o, unreal.AnimSequence))
mesh.set_editor_property('lod_settings', unreal.load_asset('/Game/LivingWorld/Models/DA_HumanLODSettings'))
assert skeletal.regenerate_lod(mesh, 4, False, False)
lib.save_loaded_asset(mesh)
man = profile_from('DA_Civilian', 'DA_CivilianMan', {
    'skeletal_mesh': mesh, 'cruise_animation': animations['Walk'], 'flee_animation': animations['Run'],
    'idle_animation': animations['Idle'], 'approved': True,
    'source': 'Tripo H3.1 text-to-3D task fd1604d5-f003-438c-8c87-aa0448c37c93, Mixamo auto rig, walk/run/idle presets',
    'license': 'Account-generated Tripo asset (user-authorized credits)',
    'scale_evidence': f"Artist-normalized standing height {manifest['height_m']} m; generated anatomy"})
blend = unreal.LivingAnimationAuthoring.build_locomotion(man, 'BS_CivilianMan')
lib.save_loaded_asset(man, False)
report['CivilianMan'] = {'mesh': mesh.get_path_name(), 'blend': blend.get_path_name() if blend else None,
                         'vertices_per_lod': [skeletal.get_num_verts(mesh, i) for i in range(4)]}

# Review renders from a temporary actor high above the editor world.
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for name, asset, distance in (('Humvee', '/Game/LivingWorld/Models/Humvee/SK_Humvee', 900), ('Sedan', '/Game/LivingWorld/Models/Sedan/SK_Sedan', 850),
                              ('Ariete', '/Game/LivingWorld/Models/Ariete/SM_Ariete', 1500), ('TB2', '/Game/LivingWorld/Models/Flight/TB2/SK_TB2', 1800),
                              ('CivilianMan', '/Game/LivingWorld/Models/CivilianMan/SK_CivilianMan', 400)):
    origin = unreal.Vector(0, 0, 60000)
    review = actors.spawn_actor_from_object(unreal.load_asset(asset), origin, unreal.Rotator(0, 0, 0))
    capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, origin + unreal.Vector(-distance, -distance, distance * .45), unreal.Rotator(-18, 45, 0))
    component = capture.capture_component2d
    target = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    component.texture_target = target
    component.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    component.fov_angle = 55
    component.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, target, str(root / 'Saved/LivingWorld'), f'{name}-unreal.png')
    actors.destroy_actor(review)
    actors.destroy_actor(capture)
(root / 'Saved/LivingWorld/new-assets-import.json').write_text(json.dumps(report, indent=2))
print('NEW_ASSETS_IMPORTED', json.dumps(report))
