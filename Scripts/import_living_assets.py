"""Run in Unreal Editor via ue_remote.py. Imports prepared drafts; never modifies source maps."""
import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
manifest = json.loads((project/'Art/LivingWorld/Prepared/manifest.json').read_text())
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
result = []
# Start with representative aircraft and drone. Remaining prepared drafts stay outside cooked content.
for entry in manifest:
    if entry['id'] not in ['F4','Orqa','UH1'] or 'error' in entry:
        continue
    folder = '/Game/LivingWorld/Models/' + entry['id'] + '_Textured'
    task = unreal.AssetImportTask()
    task.filename = entry['fbx']; task.destination_path = folder
    task.destination_name = entry['id']; task.automated = True; task.save = True
    task.replace_existing = True
    options = unreal.FbxImportUI()
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type = False
    options.import_animations = True
    options.import_materials = True; options.import_textures = True
    options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', True)
    options.skeletal_mesh_import_data.set_editor_property('import_uniform_scale', 1.0)
    task.options = options
    tools.import_asset_tasks([task])
    objects = task.get_objects()
    result.append({'id':entry['id'],'assets':[(o.get_path_name(),o.get_class().get_name()) for o in objects]})
    mesh = next((o for o in objects if isinstance(o, unreal.SkeletalMesh)),None)
    animation = next((o for o in objects if isinstance(o, unreal.AnimSequence)),None)
    if mesh:
        factory = unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.LivingAssetProfile)
        profile = unreal.load_asset('/Game/LivingWorld/Profiles/DA_'+entry['id'])
        if not profile: profile = tools.create_asset('DA_'+entry['id'],'/Game/LivingWorld/Profiles',unreal.LivingAssetProfile,factory)
        if profile:
            profile.set_editor_property('skeletal_mesh',mesh)
            if animation: profile.set_editor_property('cruise_animation',animation)
            kind = {'F4':unreal.LivingKind.PLANE,'Orqa':unreal.LivingKind.DRONE,'UH1':unreal.LivingKind.HELICOPTER}[entry['id']]
            profile.set_editor_property('kind',kind)
            profile.set_editor_property('source',entry['source'])
            profile.set_editor_property('license','User-supplied; redistribution terms unverified')
            profile.set_editor_property('scale_evidence','Orqa: scaled from 0.127 m propeller diameter; source https://orqafpv.com/products/mrm1-5' if entry['id']=='Orqa' else 'Source metres retained; inspect subtype-specific dimensions before measurement use')
            profile.set_editor_property('speed_meters_per_second', {'F4':80.,'Orqa':8.,'UH1':35.}[entry['id']])
            profile.set_editor_property('collision_radius_cm',{'F4':450.,'Orqa':16.,'UH1':350.}[entry['id']])
            profile.set_editor_property('altitude_meters',{'F4':250.,'Orqa':30.,'UH1':100.}[entry['id']])
            unreal.EditorAssetLibrary.save_loaded_asset(profile)
(project/'Saved/LivingWorld/import-report.json').write_text(json.dumps(result,indent=2))
print('LIVING_IMPORT',[(r['id'],len(r['assets'])) for r in result])
