"""Import the licensed distant-flock bird and its five baked animation clips."""
import unreal
import json
from pathlib import Path

root = Path(unreal.Paths.project_dir())
folder = '/Game/LivingWorld/Models/Pigeon'
assets = unreal.AssetToolsHelpers.get_asset_tools()
manifest = json.loads((root / 'Art/LivingWorld/Prepared/Pigeon/manifest.json').read_text())
mesh = None
skeleton = None
animations = {}
report = []
for index, clip in enumerate(manifest['clips']):
    task = unreal.AssetImportTask()
    task.filename = str(root / 'Art/LivingWorld/Prepared/Pigeon' / (clip['name'] + '.fbx'))
    task.destination_path = folder
    task.destination_name = 'SK_Pigeon' if index == 0 else clip['name']
    task.automated = True
    task.save = True
    task.replace_existing = True
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = index == 0
    options.import_textures = index == 0
    options.import_mesh = index == 0
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if index == 0 else unreal.FBXImportType.FBXIT_ANIMATION
    if skeleton:
        options.skeleton = skeleton
    # Preserve the actual rest skeleton; first animation frame is a folded wing.
    options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
    task.options = options
    assets.import_asset_tasks([task])
    imported = task.get_objects()
    report.extend([(o.get_path_name(), o.get_class().get_name()) for o in imported])
    if index == 0:
        mesh = next(o for o in imported if isinstance(o, unreal.SkeletalMesh))
        skeleton = mesh.get_editor_property('skeleton')
    animation = next((o for o in imported if isinstance(o, unreal.AnimSequence)), None)
    if not animation:
        raise RuntimeError('No animation imported: ' + clip['name'])
    animations[clip['name']] = animation

profile_path = '/Game/LivingWorld/Profiles/DA_Pigeon'
profile = unreal.load_asset(profile_path)
if not profile:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.LivingAssetProfile)
    profile = assets.create_asset('DA_Pigeon', '/Game/LivingWorld/Profiles', unreal.LivingAssetProfile, factory)
for key, value in {
    'kind': unreal.LivingKind.BIRD, 'approved': False, 'skeletal_mesh': mesh,
    'cruise_animation': animations['Pigeon_Flapping'],
    'source': 'Paul Daniel Spooner / dudecon: https://sketchfab.com/3d-models/animated-bird-pigeon-797d27b68af3453e865149435df6aa30',
    'license': 'CC BY 4.0; see Art/LivingWorld/Sourced/Pigeon/ATTRIBUTION.md',
    'scale_evidence': 'Source metres retained, approximately 0.30 m body. Artistic distant-flock LOD; bake error up to 0.0245 m.',
    'speed_meters_per_second': 8.0, 'collision_radius_cm': 18.0,
    'altitude_meters': 20.0, 'turn_rate_degrees': 90.0,
}.items():
    profile.set_editor_property(key, value)
unreal.EditorAssetLibrary.save_loaded_asset(profile)
(root / 'Saved/LivingWorld/pigeon-import.json').write_text(json.dumps(report, indent=2))
print('PIGEON_IMPORTED', report)
