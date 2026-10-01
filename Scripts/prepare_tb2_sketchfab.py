"""Prepare the Sketchfab Bayraktar TB2 (TheDevilsEye, CC Attribution) for Unreal.

blender -b --factory-startup -P Scripts/prepare_tb2_sketchfab.py
Source: Art/LivingWorld/Sourced/TB2_TheDevilsEye (downloaded by the user, 2026-10-01): one metric mesh,
12.0 m span, nose toward -Y, 4K PBR textures. Credit: "Baykar Bayraktar TB2" by TheDevilsEye
(https://sketchfab.com/3d-models/baykar-bayraktar-tb2-8e5b6972f7d049f19688096e03949487), CC BY 4.0.
The pusher propeller is part of the single mesh; its disk was located on orthographic renders and
a vertex histogram and is weighted to a spinning bone. Output replaces the generated TB2.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
source = root / 'Art/LivingWorld/Sourced/TB2_TheDevilsEye'
out = root / 'Art/LivingWorld/Prepared/Flight'
HUB = Vector((0, .66, 1.225))            # metres, propeller hub (three blades meet here)
DISK_Y = (.55, .80)                      # propeller disk and spinner, clear of the tail cone and booms
RADIUS = 1.05                            # blade tips reach 0.97 m; the booms sit at 1.12 m
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source / 'source/model/bayraktar_tb2.fbx'))
body = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
body.data.transform(body.matrix_world)
body.matrix_world.identity()
for o in list(bpy.context.scene.objects):
    if o != body:
        bpy.data.objects.remove(o, do_unlink=True)
body.name = 'TB2'
# Link the supplied PBR set (the FBX carries only the material slot).
material = body.data.materials[0]
material.use_nodes = True
tree = material.node_tree
bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')


def texture(suffix, colour):
    node = tree.nodes.new('ShaderNodeTexImage')
    node.image = bpy.data.images.load(str(source / f'textures/mat_bay_tb2_{suffix}.png'))
    node.image.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    return node


tree.links.new(texture('albedo', True).outputs['Color'], bsdf.inputs['Base Color'])
tree.links.new(texture('metallic', False).outputs['Color'], bsdf.inputs['Metallic'])
tree.links.new(texture('roughness', False).outputs['Color'], bsdf.inputs['Roughness'])
normal_map = tree.nodes.new('ShaderNodeNormalMap')
tree.links.new(texture('normal', False).outputs['Color'], normal_map.inputs['Color'])
tree.links.new(normal_map.outputs['Normal'], bsdf.inputs['Normal'])
arm = bpy.data.armatures.new('TB2Skeleton')
rig = bpy.data.objects.new('TB2Rig', arm)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
base = arm.edit_bones.new('root')
base.head, base.tail = (0, 0, 0), (0, .5, 0)
prop = arm.edit_bones.new('propeller')
prop.head = HUB
prop.tail = HUB + Vector((0, .4, 0))
prop.parent = base
bpy.ops.object.mode_set(mode='OBJECT')
groups = {b: body.vertex_groups.new(name=b) for b in ('root', 'propeller')}
counts = {'root': 0, 'propeller': 0}
for v in body.data.vertices:
    inside = DISK_Y[0] < v.co.y < DISK_Y[1] and math.hypot(v.co.x - HUB.x, v.co.z - HUB.z) < RADIUS
    bone = 'propeller' if inside else 'root'
    groups[bone].add([v.index], 1, 'REPLACE')
    counts[bone] += 1
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
report = {'id': 'TB2', 'source': 'https://sketchfab.com/3d-models/baykar-bayraktar-tb2-8e5b6972f7d049f19688096e03949487',
          'author': 'TheDevilsEye', 'license': 'CC BY 4.0 (credit: "Baykar Bayraktar TB2" by TheDevilsEye on Sketchfab)',
          'span_m': round(max(p.x for p in points) - min(p.x for p in points), 2),
          'length_m': round(max(p.y for p in points) - min(p.y for p in points), 2),
          'height_m': round(max(p.z for p in points) - min(p.z for p in points), 2), 'bone_vertices': counts,
          'source_forward': '-Y', 'scope': 'Metric source model (12 m span, published TB2 span 12 m); propeller segmented for animation'}
(out / 'TB2.json').write_text(json.dumps(report, indent=2))
print('TB2_READY', json.dumps(report))
