"""Prepare the Shahed-136 loitering munition (user-supplied OBJ) for Unreal; replaces the Geranium-2.

blender -b --factory-startup -P Scripts/prepare_shahed.py
Source: Art/LivingWorld/Sourced/Shahed136/source/Shahed Drone 136.obj (supplied 2026-10-01; a
Tripo-generated mesh with vertex colours, 1.95 M triangles, no textures). Converted to a real-time
asset: decimated to 60k triangles (colours kept as a vertex colour attribute), nose turned to -Y
(the source stands on its nose along -Z), scaled to the published 2.5 m span, and the pusher
propeller (located on orthographic renders and a vertex histogram) weighted to a spinning bone.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parents[1]
out = root / 'Art/LivingWorld/Prepared/Flight'
SPAN_M = 2.5
TRIANGLES = 60000
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(root / 'Art/LivingWorld/Sourced/Shahed136/source/Shahed Drone 136.obj'))
body = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
bpy.context.view_layer.objects.active = body
decimate = body.modifiers.new('Decimate', 'DECIMATE')
decimate.ratio = TRIANGLES / len(body.data.polygons)
bpy.ops.object.modifier_apply(modifier='Decimate')
body.data.transform(body.matrix_world)
body.matrix_world = Matrix.Identity(4)
body.name = 'Shahed136'
# Propeller in source units (after the importer's Y-up rotation): disk behind the engine block.
prop_vertices = {v.index for v in body.data.vertices if v.co.z > .947 and math.hypot(v.co.x, v.co.y) < .115}
hub_source = Vector((0, 0, .962))
points = [v.co for v in body.data.vertices]
lo = Vector([min(p[i] for p in points) for i in range(3)])
hi = Vector([max(p[i] for p in points) for i in range(3)])
scale = SPAN_M / (hi.x - lo.x)
# Nose -Z -> -Y and the -Y face (panel markings, spine) -> up: rotate -90 degrees about X.
to_unreal = Matrix.Scale(scale, 4) @ Matrix.Rotation(math.radians(-90), 4, 'X') @ Matrix.Translation(-(lo + hi) / 2)
body.data.transform(to_unreal)
hub = to_unreal @ hub_source
arm = bpy.data.armatures.new('Shahed136Skeleton')
rig = bpy.data.objects.new('Shahed136Rig', arm)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
base = arm.edit_bones.new('root')
base.head, base.tail = (0, 0, 0), (0, .3, 0)
prop = arm.edit_bones.new('propeller')
prop.head = hub
prop.tail = hub + Vector((0, .15, 0))
prop.parent = base
bpy.ops.object.mode_set(mode='OBJECT')
groups = {b: body.vertex_groups.new(name=b) for b in ('root', 'propeller')}
for v in body.data.vertices:
    groups['propeller' if v.index in prop_vertices else 'root'].add([v.index], 1, 'REPLACE')
body.modifiers.new('Propeller', 'ARMATURE').object = rig
body.parent = rig
rig.animation_data_create()
rig.animation_data.action = bpy.data.actions.new('Shahed136_Cruise')
for frame in range(61):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0, 0, 0)
        # Visual sampling rate, not engine RPM.
        if bone.name == 'propeller':
            bone.rotation_euler.y = math.tau * 4 * frame / 60
        bone.keyframe_insert('rotation_euler', frame=frame + 1)
scene = bpy.context.scene
scene.render.fps = 60
scene.frame_start, scene.frame_end = 1, 61
scene.frame_set(1)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Shahed136.blend'))
bpy.ops.export_scene.fbx(filepath=str(out / 'Shahed136.fbx'), use_selection=True, object_types={'MESH', 'ARMATURE'},
    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z', colors_type='SRGB', path_mode='COPY')
points = [v.co for v in body.data.vertices]
report = {'id': 'Shahed136', 'source': 'User-supplied shahed-136-drone.zip (Tripo-generated OBJ), 2026-10-01',
          'triangles': len(body.data.polygons), 'propeller_vertices': len(prop_vertices),
          'span_m': round(max(p.x for p in points) - min(p.x for p in points), 2),
          'length_m': round(max(p.y for p in points) - min(p.y for p in points), 2),
          'height_m': round(max(p.z for p in points) - min(p.z for p in points), 2), 'source_forward': '-Y',
          'scope': 'Scaled to the published 2.5 m span; generated proportions make it shorter than the published 3.5 m length'}
(out / 'Shahed136.json').write_text(json.dumps(report, indent=2))
print('SHAHED_READY', json.dumps(report))
