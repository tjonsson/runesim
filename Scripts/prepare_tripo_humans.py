"""Normalize bridge-imported humanoids and export three clips using Blender 5.2.

Run in background Blender, --disable-autoexec --python this-file -- Civilian.
Source libraries are written by the DCC bridge capture step; never modify them.
"""
import bpy
import json
import sys
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
kind = sys.argv[sys.argv.index('--') + 1]
assert kind in ('Civilian', 'Soldier')
target_height = 1.70 if kind == 'Civilian' else 1.80
source = root / f'Art/LivingWorld/Generated/{kind}/{kind}_Tripo_Source.blend'
out = root / f'Art/LivingWorld/Prepared/{kind}'
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source), link=False) as (available, loaded):
    loaded.objects = available.objects
    prefix = 'Armature|' if kind == 'Civilian' else 'SoldierSource_'
    loaded.actions = [n for n in available.actions if n in tuple(prefix + clip for clip in ('walk', 'run', 'idle'))]
for obj in loaded.objects:
    bpy.context.collection.objects.link(obj)
rig = next(o for o in loaded.objects if o.type == 'ARMATURE')
meshes = [o for o in loaded.objects if o.type == 'MESH']
rig.animation_data_create()
rig.animation_data.action = None
rig.animation_data.use_nla = False
rig.data.pose_position = 'REST'
bpy.context.view_layer.update()
corners = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
height = max(v.z for v in corners) - min(v.z for v in corners)
scale = target_height / height
# Uniform scene-unit scaling preserves every keyed translation and the bind pose.
# Blender scene units and FBX conversion yield centimetres in Unreal.
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = scale
bpy.context.scene.render.fps = 24
rig.data.pose_position = 'POSE'
rig.name = kind + 'Rig'
for obj in loaded.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = rig
for image in bpy.data.images:
    if image.type == 'IMAGE' and image.size[0]:
        image.filepath_raw = str(out / (image.name.replace('.', '_') + '.png'))
        image.file_format = 'PNG'
        image.save()
clips = []
for name in ('walk', 'run', 'idle'):
    action = next(a for a in loaded.actions if a.name == prefix + name)
    action.name = kind + '_' + name.title()
    action.use_fake_user = True
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    start, end = map(int, action.frame_range)
    # Bridge exports can retain travel even when Studio's in-place option is on.
    # Remove net planar travel while preserving the gait's local sway and bob.
    hips = next(b for b in rig.pose.bones if not b.parent)
    poses = []
    for frame in range(start, end + 1):
        bpy.context.scene.frame_set(frame)
        poses.append(hips.matrix.copy())
    origin = poses[0].translation.copy()
    travel = poses[-1].translation - origin
    rest = hips.bone.matrix_local.translation
    for offset, matrix in enumerate(poses):
        frame = start + offset
        bpy.context.scene.frame_set(frame)
        position = matrix.translation.copy()
        for axis in (0, 1):
            position[axis] -= origin[axis] - rest[axis] + travel[axis] * offset / max(1, end - start)
        matrix.translation = position
        hips.matrix = matrix
        hips.keyframe_insert(data_path='location', frame=frame, group=hips.name)
    bpy.context.scene.frame_start = start
    bpy.context.scene.frame_end = end
    bpy.context.scene.frame_set(start)
    path = out / (action.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True,
        object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
        bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0, apply_unit_scale=True,
        axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
    clips.append({'name': action.name, 'fbx': str(path), 'frames': [start, end], 'removed_planar_travel_m': [travel.x * scale, travel.y * scale]})
rig.animation_data.action = loaded.actions[0]
bpy.context.scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(out / (kind + '.blend')))
report = {'asset': kind, 'source_height_units': height, 'height_m': target_height,
    'unit_scale_m': scale, 'feet_z_m': min(v.z for v in corners) * scale,
    'bones': len(rig.data.bones), 'clips': clips,
    'note': 'Generated approximation; target height is an artistic normalization, not measured anatomy.'}
(out / 'manifest.json').write_text(json.dumps(report, indent=2))
print('HUMAN_PREPARED', json.dumps(report))
