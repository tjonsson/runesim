"""Import the prepared NASA Global Hawk as a Living World aircraft (editor, PIE stopped).

Input: Art/LivingWorld/Prepared/Flight/GlobalHawk.fbx from prepare_global_hawk.py.
Creates SM_GlobalHawk with four LODs and DA_GlobalHawk (Plane). Writes a review render to
Saved/LivingWorld/GlobalHawk-unreal.png so orientation and materials can be checked.
"""
import json
import unreal
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
project = Path(unreal.Paths.project_dir())
prepared = project / 'Art/LivingWorld/Prepared/Flight'
info = json.loads((prepared / 'GlobalHawk.json').read_text())
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
folder = '/Game/LivingWorld/Models/Flight/GlobalHawk'

task = unreal.AssetImportTask()
task.filename = str(prepared / 'GlobalHawk.fbx'); task.destination_path = folder; task.destination_name = 'SM_GlobalHawk'
task.automated = task.save = task.replace_existing = True
options = unreal.FbxImportUI()
options.import_mesh = True; options.import_as_skeletal = False; options.import_animations = False
options.import_materials = True; options.import_textures = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.static_mesh_import_data.set_editor_property('combine_meshes', True)
options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
task.options = options
tools.import_asset_tasks([task])
mesh = unreal.load_asset(folder + '/SM_GlobalHawk')
assert isinstance(mesh, unreal.StaticMesh), 'Import failed'

subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
reduction = unreal.EditorScriptingMeshReductionOptions()
reduction.reduction_settings = [unreal.EditorScriptingMeshReductionSettings(percent, screen)
                                for percent, screen in ((1., 1.), (.5, .3), (.2, .12), (.06, .035))]
reduction.auto_compute_lod_screen_size = False
lods = subsystem.set_lods_with_notification(mesh, reduction, False)
bounds = mesh.get_bounding_box()
extent = bounds.max - bounds.min
assert extent.y > extent.x > extent.z, f'Span must lie along Y and length along X: {extent}'
lib.save_loaded_asset(mesh, False)

path = '/Game/LivingWorld/Profiles/DA_GlobalHawk'
profile = unreal.load_asset(path)
if not profile:
    factory = unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class', unreal.LivingAssetProfile)
    profile = tools.create_asset('DA_GlobalHawk', '/Game/LivingWorld/Profiles', unreal.LivingAssetProfile, factory)
jet = unreal.load_asset('/Game/LivingWorld/Audio/Ambient/jet_loop')
values = {
    'kind': unreal.LivingKind.PLANE, 'static_mesh': mesh, 'skeletal_mesh': None, 'approved': True,
    # The exported nose faces -X in Unreal (verified in the review renders).
    'visual_scale': unreal.Vector(1, 1, 1), 'visual_rotation': unreal.Rotator(roll=0, pitch=0, yaw=180), 'visual_offset': unreal.Vector(),
    # Scenery tuning for a high-endurance UAV in the local airspace, not RQ-4 performance data.
    'speed_meters_per_second': 60., 'altitude_meters': 650., 'collision_radius_cm': 1800.,
    'flight_radius_fraction': .95, 'turn_rate_degrees': 10., 'max_bank_degrees': 22., 'loop_sound': jet,
    'source': info['source'], 'license': 'NASA media usage guidelines; credit NASA / Michael D. Carbajal; no endorsement implied',
    'scale_evidence': f"Scaled to NASA RQ-4A published span 35.4 m; model length reads {info['length_m']} m (published 13.5 m) - source proportions differ",
}
for key, value in values.items():
    profile.set_editor_property(key, value)
profile.modify()
lib.save_loaded_asset(profile, False)

# Review render: temporary actor and camera in the current editor world, removed afterwards.
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
origin = unreal.Vector(0, 0, 60000)
review = actors.spawn_actor_from_object(mesh, origin, unreal.Rotator(0, 0, 0))
capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, origin + unreal.Vector(-2600, -2600, 1400), unreal.Rotator(-18, 45, 0))
component = capture.capture_component2d
target = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
component.texture_target = target
component.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
component.fov_angle = 60
component.capture_scene()
unreal.RenderingLibrary.export_render_target(world, target, str(project / 'Saved/LivingWorld'), 'GlobalHawk-unreal.png')
actors.destroy_actor(review); actors.destroy_actor(capture)
report = {'mesh': mesh.get_path_name(), 'lods': lods, 'extent_cm': [extent.x, extent.y, extent.z], 'profile': path,
          'render': 'Saved/LivingWorld/GlobalHawk-unreal.png'}
(project / 'Saved/LivingWorld/globalhawk-import.json').write_text(json.dumps(report, indent=2))
print('GLOBALHAWK_IMPORTED', json.dumps(report))
