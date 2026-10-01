"""Import the October 1 aircraft (editor, PIE stopped): Sketchfab TB2, Shahed-136 and the FPV strike drone.

Inputs: Prepared/Flight/{TB2,Shahed136,FPVDrone}.fbx from prepare_tb2_sketchfab.py, prepare_shahed.py and
prepare_fpv_drone.py. The TB2 folder is rebuilt, so the generated Tripo TB2 is replaced. The Shahed keeps
its vertex colours through a small vertex-colour material. Profiles copy the closest reviewed profile
(Global Hawk for fixed wings, Mavic 3 for the quad) and then set size, speed and sound.
"""
import json
from pathlib import Path
import unreal

assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
root = Path(unreal.Paths.project_dir())
art = root / 'Art/LivingWorld/Prepared/Flight'
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
skeletal = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
lod_settings = unreal.load_asset('/Game/LivingWorld/Models/Flight/DA_FlightLODSettings')
report = {}


def import_skeletal(name, vertex_colours=False):
    folder = '/Game/LivingWorld/Models/Flight/' + name
    if lib.does_directory_exist(folder):
        lib.delete_directory(folder)  # Replace earlier versions (e.g. the generated TB2) completely.
    task = unreal.AssetImportTask()
    task.filename = str(art / f'{name}.fbx')
    task.destination_path = folder
    task.destination_name = 'SK_' + name
    task.automated = task.save = task.replace_existing = True
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = options.import_textures = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
    if vertex_colours:
        options.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task.options = options
    tools.import_asset_tasks([task])
    assets = [a.get_asset() for a in registry.get_assets_by_path(folder)]
    mesh = next(a for a in assets if isinstance(a, unreal.SkeletalMesh))
    clip = next(a for a in assets if isinstance(a, unreal.AnimSequence))
    mesh.set_editor_property('lod_settings', lod_settings)
    assert skeletal.regenerate_lod(mesh, 4, False, False), name
    lib.save_loaded_asset(mesh)
    return mesh, clip


def vertex_colour_material():
    path = '/Game/LivingWorld/Models/Flight/M_VertexColour'
    material = unreal.load_asset(path)
    if material:
        return material
    material = tools.create_asset('M_VertexColour', '/Game/LivingWorld/Models/Flight', unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    colour = edit.create_material_expression(material, unreal.MaterialExpressionVertexColor, -300, 0)
    edit.connect_material_property(colour, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = edit.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 200)
    rough.set_editor_property('r', .55)
    edit.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(material)
    lib.save_loaded_asset(material)
    return material


def profile_from(template, name, values):
    path = '/Game/LivingWorld/Profiles/' + name
    profile = unreal.load_asset(path)
    if not profile:
        # A fresh duplicate ignores cleared soft references until it has been saved and reloaded.
        lib.save_loaded_asset(lib.duplicate_asset('/Game/LivingWorld/Profiles/' + template, path), False)
        profile = unreal.load_asset(path)
    profile.modify()
    for key, value in values.items():
        profile.set_editor_property(key, value)
    for key, value in values.items():
        assert value is not None or profile.get_editor_property(key) is None, (name, key)
    lib.save_loaded_asset(profile, False)
    return profile


def sound(name):
    return unreal.load_asset('/Game/LivingWorld/Audio/Ambient/' + name)


# Sketchfab TB2 replaces the generated one (same profile, new mesh and provenance).
tb2 = json.loads((art / 'TB2.json').read_text())
mesh, clip = import_skeletal('TB2')
profile_from('DA_GlobalHawk', 'DA_TB2', {
    'kind': unreal.LivingKind.PLANE, 'static_mesh': None, 'skeletal_mesh': mesh, 'cruise_animation': clip, 'approved': True,
    'visual_rotation': unreal.Rotator(yaw=-90), 'visual_scale': unreal.Vector(1, 1, 1), 'visual_offset': unreal.Vector(),
    # Scenery tuning for a MALE drone in the local airspace, not Bayraktar performance data.
    'speed_meters_per_second': 36., 'altitude_meters': 450., 'collision_radius_cm': 600., 'turn_rate_degrees': 14.,
    'max_bank_degrees': 25., 'loop_sound': sound('utility_engine_loop'),
    'source': tb2['source'] + ' (author ' + tb2['author'] + ')', 'license': tb2['license'],
    'scale_evidence': f"Metric source model: span {tb2['span_m']} m (published 12 m), length {tb2['length_m']} m"})
report['TB2'] = [skeletal.get_num_verts(mesh, i) for i in range(4)]

# Shahed-136: fixed-wing one-way attack drone, flown in the Drones population.
shahed = json.loads((art / 'Shahed136.json').read_text())
mesh, clip = import_skeletal('Shahed136', vertex_colours=True)
material = vertex_colour_material()
materials = mesh.get_editor_property('materials')
for slot in materials:
    slot.set_editor_property('material_interface', material)
mesh.set_editor_property('materials', materials)
lib.save_loaded_asset(mesh)
profile_from('DA_GlobalHawk', 'DA_Shahed136', {
    'kind': unreal.LivingKind.DRONE, 'static_mesh': None, 'skeletal_mesh': mesh, 'cruise_animation': clip, 'approved': True,
    'visual_rotation': unreal.Rotator(yaw=-90), 'visual_scale': unreal.Vector(1, 1, 1), 'visual_offset': unreal.Vector(),
    # Published cruise about 185 km/h; flown low over the scene at scenery altitude.
    'speed_meters_per_second': 50., 'altitude_meters': 150., 'collision_radius_cm': 150., 'turn_rate_degrees': 12.,
    'max_bank_degrees': 35., 'flight_radius_fraction': .9, 'target_health': 0., 'loop_sound': sound('utility_engine_loop'),
    'source': shahed['source'], 'license': 'User-supplied model (rights confirmed by the user)', 'scale_evidence': shahed['scope']})
report['Shahed136'] = [skeletal.get_num_verts(mesh, i) for i in range(4)]

# FPV strike quadcopter with payload.
fpv = json.loads((art / 'FPVDrone.json').read_text())
mesh, clip = import_skeletal('FPVDrone')
profile_from('DA_Mavic3', 'DA_FPVDrone', {
    'kind': unreal.LivingKind.DRONE, 'static_mesh': None, 'skeletal_mesh': mesh, 'cruise_animation': clip, 'approved': True,
    'visual_rotation': unreal.Rotator(yaw=-90), 'visual_scale': unreal.Vector(1, 1, 1), 'visual_offset': unreal.Vector(),
    'speed_meters_per_second': 28., 'altitude_meters': 35., 'collision_radius_cm': 30., 'turn_rate_degrees': 60.,
    'max_bank_degrees': 40., 'target_health': 0., 'loop_sound': sound('drone_loop'),
    'source': fpv['source'], 'license': 'User-supplied model (rights confirmed by the user)', 'scale_evidence': fpv['scope']})
report['FPVDrone'] = [skeletal.get_num_verts(mesh, i) for i in range(4)]
(root / 'Saved/LivingWorld/air-batch-import.json').write_text(json.dumps(report, indent=2))
print('AIR_BATCH', json.dumps(report))
