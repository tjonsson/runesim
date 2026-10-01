"""Prepare the generated Bayraktar TB2-style drone: metric scale, forward -Y, spinning pusher propeller.

blender -b --factory-startup -P Scripts/prepare_tb2.py
Source: Art/LivingWorld/Generated/TB2/TB2_Tripo.glb (Tripo H3.1 image-to-3D from the project reference
Art/LivingWorld/References/tb2.png). Propeller hub and radius were measured on orthographic renders.
Scaled to the published 12 m span; the generated proportions are shorter-winged, so length reads long.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parents[1]
out = root / 'Art/LivingWorld/Prepared/Flight'
SPAN_M = 12.0
HUB = Vector((.212, 0, .1437))   # source units, nose toward -X
PROP_RADIUS = .13
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(root / 'Art/LivingWorld/Generated/TB2/TB2_Tripo.glb'))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in meshes:
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
body.name = 'TB2'
for o in list(bpy.context.scene.objects):
    if o != body:
        bpy.data.objects.remove(o, do_unlink=True)
ys = [v.co.y for v in body.data.vertices]
scale = SPAN_M / (max(ys) - min(ys))
# Nose -X -> -Y, matching the ground vehicles' import convention.
turn = Matrix.Rotation(math.radians(90), 4, 'Z')
arm = bpy.data.armatures.new('TB2Skeleton')
rig = bpy.data.objects.new('TB2Rig', arm)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
base = arm.edit_bones.new('root')
base.head, base.tail = (0, 0, 0), (0, .5, 0)
prop = arm.edit_bones.new('propeller')
prop.head = (turn @ HUB) * scale
prop.tail = prop.head + Vector((0, .4, 0))
prop.parent = base
bpy.ops.object.mode_set(mode='OBJECT')
groups = {b: body.vertex_groups.new(name=b) for b in ('root', 'propeller')}
counts = {'root': 0, 'propeller': 0}
for v in body.data.vertices:
    # The blades rest vertical: a thin slab behind the tail cone, clear of the booms (|y| ~ .105) and tail (x > .34).
    bone = 'propeller' if .192 < v.co.x < .24 and abs(v.co.y - HUB.y) < .085 and abs(v.co.z - HUB.z) < PROP_RADIUS else 'root'
    groups[bone].add([v.index], 1, 'REPLACE')
    counts[bone] += 1
    v.co = (turn @ v.co) * scale
body.modifiers.new('Propeller', 'ARMATURE').object = rig
body.parent = rig
rig.animation_data_create()
rig.animation_data.action = bpy.data.actions.new('TB2_Cruise')
for frame in range(61):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0, 0, 0)
        # Visual sampling rate, not engine RPM: three turns per second avoids strobing at 30 fps.
        if bone.name == 'propeller':
            bone.rotation_euler.y = math.tau * 3 * frame / 60
        bone.keyframe_insert('rotation_euler', frame=frame + 1)
textures = out / 'textures/TB2'
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
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'TB2.blend'))
bpy.ops.export_scene.fbx(filepath=str(out / 'TB2.fbx'), use_selection=True, object_types={'MESH', 'ARMATURE'},
    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
points = [v.co for v in body.data.vertices]
report = {'id': 'TB2', 'source_task': '502a3497-2c68-4ee1-8365-c8f54b0b46e6', 'reference': 'Art/LivingWorld/References/tb2.png',
          'span_m': round(max(p.x for p in points) - min(p.x for p in points), 2),
          'length_m': round(max(p.y for p in points) - min(p.y for p in points), 2),
          'height_m': round(max(p.z for p in points) - min(p.z for p in points), 2), 'bone_vertices': counts,
          'source_forward': '-Y', 'license': 'Account-generated Tripo asset (user-authorized credits)',
          'scope': 'Bayraktar TB2-style generated approximation; scaled to 12 m published span, length reads long (published 6.5 m)'}
(out / 'TB2.json').write_text(json.dumps(report, indent=2))
print('TB2_READY', json.dumps(report))
