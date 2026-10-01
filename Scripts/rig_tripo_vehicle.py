"""Articulate the four wheels of a generated four-wheel vehicle (Blender 5.2, background).

blender -b --factory-startup -P Scripts/rig_tripo_vehicle.py -- Humvee
Wheel centres, radius and track were measured on orthographic renders of the unchanged
Tripo GLB (forward -Y, Z up, source units). Lengths are project art normalizations.
"""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

VEHICLES = {
    # Tripo H3.1 text-to-3D, 2026-09-30. Length: published M1151 (4.93 m).
    'Humvee': {'task': '7eb00043-1659-4fe2-8f38-3ff420335e71', 'length_m': 4.93,
               'front_y': -.3634, 'rear_y': .3023, 'axle_z': .1068, 'radius': .1022, 'track_x': .181, 'inner_x': .140,
               'scope': 'HMMWV-style US utility vehicle (M1151 reference); generated shape, two-tone paint, not a verified variant'},
    # Tripo H3.1 text-to-3D, 2026-09-30. Length: typical mid-size sedan (4.7 m).
    'Sedan': {'task': '16c752e1-3bff-49f5-bda5-18ba6c408812', 'length_m': 4.70,
              'front_y': -.2948, 'rear_y': .2790, 'axle_z': .0775, 'radius': .0747, 'track_x': .1695, 'inner_x': .143,
              'scope': 'Generic civilian mid-size sedan; generated grille badge resembles a real maker badge'},
}
name = sys.argv[sys.argv.index('--') + 1]
spec = VEHICLES[name]
root = Path(__file__).resolve().parents[1]
out = root / 'Art/LivingWorld/Prepared/Ground'
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(root / f'Art/LivingWorld/Generated/{name}/{name}_Tripo.glb'))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in meshes:
    for v in o.data.vertices:
        v.co = o.matrix_world @ v.co
    o.matrix_world.identity()
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
mesh = bpy.context.view_layer.objects.active
mesh.name = name
for o in list(bpy.context.scene.objects):
    if o != mesh:
        bpy.data.objects.remove(o, do_unlink=True)
ys = [v.co.y for v in mesh.data.vertices]
scale = spec['length_m'] / (max(ys) - min(ys))
centers = {side + end: Vector((sign * spec['track_x'], y, spec['axle_z']))
           for side, sign in (('L', -1), ('R', 1)) for end, y in (('F', spec['front_y']), ('R', spec['rear_y']))}
arm = bpy.data.armatures.new(name + 'Skeleton')
rig = bpy.data.objects.new(name + 'Rig', arm)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
base = arm.edit_bones.new('root')
base.head = (0, 0, 0)
base.tail = (0, .3, 0)
for wheel, center in centers.items():
    bone = arm.edit_bones.new('wheel_' + wheel)
    bone.head = center * scale
    bone.tail = bone.head + Vector((.2, 0, 0))
    bone.parent = base
bpy.ops.object.mode_set(mode='OBJECT')
groups = {bone: mesh.vertex_groups.new(name=bone) for bone in arm.bones.keys()}
counts = {bone: 0 for bone in groups}
for v in mesh.data.vertices:
    bone = 'root'
    for wheel, c in centers.items():
        if v.co.x * c.x > 0 and abs(v.co.x) > spec['inner_x'] and math.hypot(v.co.y - c.y, v.co.z - c.z) < spec['radius'] * 1.02:
            bone = 'wheel_' + wheel
            break
    groups[bone].add([v.index], 1, 'REPLACE')
    counts[bone] += 1
    v.co *= scale
mesh.modifiers.new('WheelArticulation', 'ARMATURE').object = rig
mesh.parent = rig
rig.animation_data_create()
action = bpy.data.actions.new(name + '_Driving')
rig.animation_data.action = action
for frame in range(61):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0, 0, 0)
        # Local Y follows the axle; one revolution per second, scaled at runtime by speed.
        if bone.name.startswith('wheel_'):
            bone.rotation_euler.y = -math.tau * frame / 60
        bone.keyframe_insert('rotation_euler', frame=frame + 1)
textures = out / f'textures/{name}'
textures.mkdir(parents=True, exist_ok=True)
for i, img in enumerate(bpy.data.images):
    if img.type == 'IMAGE' and len(img.pixels):
        img.filepath_raw = str(textures / f'texture_{i}.png')
        img.file_format = 'PNG'
        img.save()
scene = bpy.context.scene
scene.render.fps = 60
scene.frame_start, scene.frame_end = 1, 61
scene.frame_set(1)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.wm.save_as_mainfile(filepath=str(out / f'{name}.blend'))
bpy.ops.export_scene.fbx(filepath=str(out / f'{name}.fbx'), use_selection=True, object_types={'MESH', 'ARMATURE'},
    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
xs = [v.co.x for v in mesh.data.vertices]
zs = [v.co.z for v in mesh.data.vertices]
report = {'id': name, 'source_task': spec['task'], 'length_m': spec['length_m'], 'width_m': round(max(xs) - min(xs), 3),
          'height_m': round(max(zs) - min(zs), 3), 'wheel_radius_m': spec['radius'] * scale,
          'wheelbase_m': (spec['rear_y'] - spec['front_y']) * scale, 'bone_vertices': counts, 'source_forward': '-Y',
          'license': 'Account-generated Tripo asset (user-authorized credits)', 'scope': spec['scope']}
(out / f'{name}.json').write_text(json.dumps(report, indent=2))
print('VEHICLE_RIG_READY', json.dumps(report))
