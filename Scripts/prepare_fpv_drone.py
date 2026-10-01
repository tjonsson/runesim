"""Prepare the user-supplied FPV strike drone (quadcopter with payload) for Unreal.

blender -b --factory-startup -P Scripts/prepare_fpv_drone.py
Source: Art/LivingWorld/Sourced/FPVDrone/source/FPV Drone.obj (supplied 2026-10-01): metric, nose -Y,
separate parts (fuselage, arms, four propellers, gear) and two PBR texture sets. The four propellers
become bones spinning at their own centres (diagonal pairs counter-rotate); the magenta placeholder
part is dropped. Output: Prepared/Flight/FPVDrone.fbx.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
source = root / 'Art/LivingWorld/Sourced/FPVDrone/source'
out = root / 'Art/LivingWorld/Prepared/Flight'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(source / 'FPV Drone.obj'))
parts = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in parts:
    o.data.transform(o.matrix_world)
    o.matrix_world.identity()
# Rebuild the two texture sets as PBR materials (the OBJ's map_normal/map_ao are not imported).
TEXTURES = {'5d500388-5ad8-4a97-84d7-c5dd5bd87d67': 'fpv_kamikaze_drone_1', '94d7499c-18e5-4526-97d9-ea31b09f77cf': 'fpv_kamikaze_drone'}
for material in bpy.data.materials:
    stem = TEXTURES.get(material.name.split('.')[0])
    if not stem:
        continue
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    colour = tree.nodes.new('ShaderNodeTexImage')
    colour.image = bpy.data.images.load(str(source / f'{stem}_c.jpg'))
    tree.links.new(colour.outputs['Color'], bsdf.inputs['Base Color'])
    normal = tree.nodes.new('ShaderNodeTexImage')
    normal.image = bpy.data.images.load(str(source / f'{stem}_n_n.jpg'))
    normal.image.colorspace_settings.name = 'Non-Color'
    normal_map = tree.nodes.new('ShaderNodeNormalMap')
    tree.links.new(normal.outputs['Color'], normal_map.inputs['Color'])
    tree.links.new(normal_map.outputs['Normal'], bsdf.inputs['Normal'])
    bsdf.inputs['Roughness'].default_value = .55
    material.name = 'M_FPV_' + ('Body' if stem.endswith('_1') else 'Detail')
props = sorted([o for o in parts if o.name.startswith('vehicle#prop01_')], key=lambda o: o.name)
assert len(props) == 4, [o.name for o in parts]
for o in [o for o in parts if o.name.startswith('vehicle#gear_c')]:
    parts.remove(o)
    bpy.data.objects.remove(o, do_unlink=True)
arm = bpy.data.armatures.new('FPVDroneSkeleton')
rig = bpy.data.objects.new('FPVDroneRig', arm)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
base = arm.edit_bones.new('root')
base.head, base.tail = (0, 0, 0), (0, .1, 0)
centres = {}
for index, part in enumerate(props, start=1):
    points = [v.co for v in part.data.vertices]
    centre = Vector([(min(p[i] for p in points) + max(p[i] for p in points)) / 2 for i in range(3)])
    centres['propeller_' + str(index)] = centre
    bone = arm.edit_bones.new('propeller_' + str(index))
    bone.head = centre
    bone.tail = centre + Vector((0, 0, .05))  # Bone Y = spin axis (up).
    bone.parent = base
bpy.ops.object.mode_set(mode='OBJECT')
for part in parts:
    group = part.vertex_groups.new(name='propeller_' + str(props.index(part) + 1) if part in props else 'root')
    group.add([v.index for v in part.data.vertices], 1, 'REPLACE')
bpy.ops.object.select_all(action='DESELECT')
for part in parts:
    part.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
body.name = 'FPVDrone'
body.modifiers.new('Propellers', 'ARMATURE').object = rig
body.parent = rig
rig.animation_data_create()
rig.animation_data.action = bpy.data.actions.new('FPVDrone_Cruise')
for frame in range(61):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0, 0, 0)
        if bone.name.startswith('propeller_'):
            # Diagonal pairs counter-rotate; five turns per second is a visual rate, not motor RPM.
            direction = 1 if bone.name[-1] in '14' else -1
            bone.rotation_euler.y = direction * math.tau * 5 * frame / 60
        bone.keyframe_insert('rotation_euler', frame=frame + 1)
scene = bpy.context.scene
scene.render.fps = 60
scene.frame_start, scene.frame_end = 1, 61
scene.frame_set(1)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'FPVDrone.blend'))
bpy.ops.export_scene.fbx(filepath=str(out / 'FPVDrone.fbx'), use_selection=True, object_types={'MESH', 'ARMATURE'},
    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
points = [v.co for v in body.data.vertices]
report = {'id': 'FPVDrone', 'source': 'User-supplied fpv-drone.zip (OBJ with PBR textures), 2026-10-01',
          'triangles': sum(len(p.vertices) - 2 for p in body.data.polygons),
          'width_m': round(max(p.x for p in points) - min(p.x for p in points), 3),
          'length_m': round(max(p.y for p in points) - min(p.y for p in points), 3),
          'height_m': round(max(p.z for p in points) - min(p.z for p in points), 3),
          'propeller_centres_m': {k: [round(c, 3) for c in v] for k, v in centres.items()}, 'source_forward': '-Y',
          'scope': 'Source scale kept (about 0.48 m across the propellers, a 7-10 inch strike quad)'}
(out / 'FPVDrone.json').write_text(json.dumps(report, indent=2))
print('FPV_READY', json.dumps(report))
