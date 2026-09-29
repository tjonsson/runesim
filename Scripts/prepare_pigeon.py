"""Bake source Rigify controls into a portable deformation-only bird skeleton.

Run in background Blender with --disable-autoexec. Source rig UI is never run.
"""
import bpy
import json
from pathlib import Path
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parents[1]
out = root / 'Art/LivingWorld/Prepared/Pigeon'
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(root / 'Art/LivingWorld/Sourced/Pigeon/bird-original.blend'), use_scripts=False)
source = bpy.data.objects['rig']
bird = bpy.data.objects['Bird']
clips = ['Flapping', 'Gliding', 'Standing Idle', 'Takeoff', 'Landing']
source.animation_data.use_nla = False
# Capture evaluated matrices first, so no source constraints/drivers enter runtime.
names = [b.name for b in source.data.bones if b.use_deform]
samples = {}
reference_vertices = {}
for clip in clips:
    action = bpy.data.actions[clip]
    source.animation_data.action = action
    frames = range(int(action.frame_range[0]), int(action.frame_range[1]) + 1)
    samples[clip] = []
    for frame in frames:
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()
        evaluated = source.evaluated_get(bpy.context.evaluated_depsgraph_get())
        samples[clip].append({name: evaluated.pose.bones[name].matrix.copy() for name in names})
        if frame in [frames.start, frames.start + len(frames) // 2, frames.stop - 1]:
            evaluated_bird = bird.evaluated_get(bpy.context.evaluated_depsgraph_get())
            reference_vertices[(clip, frame)] = [v.co.copy() for v in evaluated_bird.data.vertices]

armature = bpy.data.armatures.new('PigeonSkeleton')
rig = bpy.data.objects.new('PigeonRig', armature)
bpy.context.collection.objects.link(rig)
rig.matrix_world = source.matrix_world.copy()
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for name in names:
    original = source.data.bones[name]
    bone = armature.edit_bones.new(name)
    bone.length = max(original.length, .001)
    bone.matrix = original.matrix_local.copy()
# Flat deformation hierarchy avoids introducing shear when Rigify control parents
# with non-uniform scale are removed. Every sampled transform is relative to root.
# One explicit root is required by Unreal, including disconnected feather bones.
root_bone = armature.edit_bones.new('root')
root_bone.head = (0, 0, 0)
root_bone.tail = (0, .05, 0)
for name in names:
    if not armature.edit_bones[name].parent:
        armature.edit_bones[name].parent = root_bone
bpy.ops.object.mode_set(mode='OBJECT')
rig.animation_data_create()
baked = []
for clip, frames in samples.items():
    action = bpy.data.actions.new('Pigeon_' + clip.replace(' ', '_'))
    rig.animation_data.action = action
    for frame_index, matrices in enumerate(frames, 1):
        for bone in rig.pose.bones:
            bone.rotation_mode = 'QUATERNION'
            if bone.name == 'root':
                bone.matrix_basis = Matrix.Identity(4)
            else:
                parent_matrix = matrices.get(bone.parent.name, Matrix.Identity(4))
                bone.matrix_basis = bone.bone.convert_local_to_pose(
                    matrices[bone.name], bone.bone.matrix_local,
                    parent_matrix=parent_matrix,
                    parent_matrix_local=bone.parent.bone.matrix_local,
                    invert=True)
            for prop in ['location', 'rotation_quaternion', 'scale']:
                bone.keyframe_insert(data_path=prop, frame=frame_index, group=bone.name)
    action.use_fake_user = True
    baked.append(action)

world = bird.matrix_world.copy()
bird.parent = rig
bird.matrix_world = world
for modifier in bird.modifiers:
    if modifier.type == 'ARMATURE':
        modifier.object = rig
max_error = 0.0
for (clip, frame), vertices in reference_vertices.items():
    rig.animation_data.action = next(a for a in baked if a.name == 'Pigeon_' + clip.replace(' ', '_'))
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    evaluated_bird = bird.evaluated_get(bpy.context.evaluated_depsgraph_get())
    assert len(vertices) == len(evaluated_bird.data.vertices)
    max_error = max(max_error, max((v.co - expected).length for v, expected in zip(evaluated_bird.data.vertices, vertices)))
# Blender B-Bone curvature is not representable by this compact FBX skeleton.
# Retain the measured approximation in the manifest and restrict use to distant
# flocks. Three centimetres is the explicit distant-LOD acceptance budget.
assert max_error < 0.03, f'Distant bird bake exceeds 3 cm error budget: {max_error}'
for image in bpy.data.images:
    if image.packed_file:
        image.filepath_raw = str(out / 'Pigeon_BaseColor.png')
        image.file_format = 'PNG'
        image.save()
# Only deliver the render mesh and portable rig, stripping downloaded scripts.
for obj in list(bpy.data.objects):
    if obj not in [bird, rig]:
        bpy.data.objects.remove(obj, do_unlink=True)
for text in list(bpy.data.texts):
    bpy.data.texts.remove(text)
for action in list(bpy.data.actions):
    if action not in baked:
        bpy.data.actions.remove(action)
rig.animation_data.action = baked[0]
scene = bpy.context.scene
scene.frame_start = 1
scene.frame_end = len(samples['Flapping'])
scene.frame_set(1)
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
bpy.ops.object.select_all(action='DESELECT')
bird.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Pigeon.blend'))
report = {'bones': len(rig.data.bones), 'vertices': len(bird.data.vertices),
          'max_deformation_error_m': max_error,
          'quality_scope': 'Distant flock LOD; B-Bone curvature approximated by rigid bone transforms',
          'base_triangles': sum(len(p.vertices)-2 for p in bird.data.polygons),
          'export_triangles': sum(len(p.vertices)-2 for p in bird.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.polygons), 'clips': []}
for action in baked:
    rig.animation_data.action = action
    scene.frame_end = int(action.frame_range[1])
    scene.frame_set(1)
    path = out / (action.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True,
        object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
        axis_forward='-Y', axis_up='Z', use_armature_deform_only=True,
        bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0.0, path_mode='COPY', embed_textures=True)
    report['clips'].append({'name': action.name, 'frames': int(action.frame_range[1]), 'fbx': str(path)})
(out / 'manifest.json').write_text(json.dumps(report, indent=2))
print('PIGEON_BAKED', json.dumps(report))
