"""Import prepared humanoids; leave profiles unapproved until visual review."""
import unreal
import json
from pathlib import Path
root = Path(unreal.Paths.project_dir())
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before import'
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []
for kind in ('Civilian', 'Soldier'):
    manifest_path = root / f'Art/LivingWorld/Prepared/{kind}/manifest.json'
    if not manifest_path.exists():
        continue
    manifest = json.loads(manifest_path.read_text())
    folder = '/Game/LivingWorld/Models/' + kind
    skeleton = mesh = None
    animations = {}
    for index, clip in enumerate([manifest['clips'][0]] + manifest['clips']):
        task = unreal.AssetImportTask()
        task.filename = str(manifest_path.parent / (clip['name'] + '.fbx'))
        task.destination_path = folder
        task.destination_name = 'SK_' + kind if index == 0 else clip['name']
        task.automated = task.save = task.replace_existing = True
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_as_skeletal = True
        options.import_animations = index > 0
        options.import_mesh = options.import_materials = options.import_textures = index == 0
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if index == 0 else unreal.FBXImportType.FBXIT_ANIMATION
        options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
        if skeleton:
            options.skeleton = skeleton
        task.options = options
        tools.import_asset_tasks([task])
        imported = task.get_objects()
        if index == 0:
            mesh = next(o for o in imported if isinstance(o, unreal.SkeletalMesh))
            skeleton = mesh.get_editor_property('skeleton')
            report.extend([o.get_path_name() for o in imported])
            continue
        animation = next(o for o in imported if isinstance(o, unreal.AnimSequence))
        animations[clip['name'].split('_')[-1]] = animation
        report.extend([o.get_path_name() for o in imported])
    profile = unreal.load_asset('/Game/LivingWorld/Profiles/DA_' + kind)
    if not profile:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.LivingAssetProfile)
        profile = tools.create_asset('DA_' + kind, '/Game/LivingWorld/Profiles', unreal.LivingAssetProfile, factory)
    for key, value in {
        'kind': unreal.LivingKind.CIVILIAN if kind == 'Civilian' else unreal.LivingKind.SOLDIER,
        'approved': False, 'skeletal_mesh': mesh, 'cruise_animation': animations['Walk'],
        'flee_animation': animations['Run'], 'visual_offset': unreal.Vector(0,0,-90),
        'idle_animation': animations['Idle'],
        'visual_rotation': unreal.Rotator(yaw=-90), 'ground_clearance_cm': 90.0,
        'speed_meters_per_second': 1.4, 'collision_radius_cm': 30.0,
        'source': 'Tripo H3.1 from project-generated reference; see Art/LivingWorld/tripo-credit-ledger.json',
        'license': 'Account-generated Tripo asset; retain provider terms with distribution review.',
        'scale_evidence': f'Artist-normalized standing height {manifest["height_m"]} m; generated anatomy.',
    }.items():
        profile.set_editor_property(key, value)
    unreal.EditorAssetLibrary.save_loaded_asset(profile)
    print('HUMAN_IMPORTED', kind, mesh.get_bounds())
(root/'Saved/LivingWorld/human-import.json').write_text(json.dumps(report,indent=2))
