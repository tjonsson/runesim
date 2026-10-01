"""Author landing, perched and takeoff clips for the generated flight-rig birds (Blender background).

Usage: blender -b --factory-startup -P Scripts/add_bird_perch_clips.py -- DetailedPigeon Gull Crow

Poses were chosen from rendered previews: bone-local X raises/lowers a wing (+ is up),
local Z sweeps it (- folds the left wing back; the right side is mirrored). The meshes
are modeled in flight posture with tucked legs, so the perched pose reads as a resting
(sitting) bird; it is intended for sensor-camera distances, not close inspection.
"""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

PROJECT = Path(__file__).resolve().parents[1]
FLIGHT = PROJECT / 'Art/LivingWorld/Prepared/Flight'
FPS = 60
# Per-species resting poses from preview iteration: (folded pose, body pitch in degrees).
FOLDS = {
    'Crow': ({'wing': (0, 0, -75), 'tip': (-25, 0, -65)}, 0.0),
    'Gull': ({'wing': (-8, 0, -86), 'tip': (-10, 0, -35)}, 0.0),          # wingtips cross over the tail
    'DetailedPigeon': ({'wing': (-60, 0, -75), 'tip': (-35, 0, -45)}, 35.0),  # model is posed upright
}
GLIDE = {'wing': (0, 0, 0), 'tip': (0, 0, 0)}
FLARE = {'wing': (40, 0, 20), 'tip': (15, 0, 0)}
UP = {'wing': (50, 0, 5), 'tip': (20, 0, 0)}
DOWN = {'wing': (-35, 0, -5), 'tip': (-15, 0, 0)}


def apply(rig, pose, root_pitch=0.0, root_lift=0.0):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0, 0, 0)
        bone.location = (0, 0, 0)
    for side, sign in (('L', 1), ('R', -1)):
        for key in ('wing', 'tip'):
            x, y, z = pose[key]
            rig.pose.bones[f'{key}_{side}'].rotation_euler = (math.radians(x), math.radians(y * sign), math.radians(z * sign))
    root = rig.pose.bones['root']
    root.rotation_euler = (math.radians(root_pitch), 0, 0)
    root.location = (0, 0, root_lift)


def blend(a, b, t):
    return {k: tuple(a[k][i] + (b[k][i] - a[k][i]) * t for i in range(3)) for k in a}


def key(rig, frame):
    for bone in rig.pose.bones:
        bone.keyframe_insert('rotation_euler', frame=frame)
        bone.keyframe_insert('location', frame=frame)


def author(name):
    FOLDED, PITCH = FOLDS[name]
    bpy.ops.wm.open_mainfile(filepath=str(FLIGHT / (name + '.blend')))
    rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    rig.animation_data_create()
    for clip in ('Landing', 'Perched', 'TakeOff'):
        old = bpy.data.actions.get(name + '_' + clip)
        if old:
            bpy.data.actions.remove(old)
    # Landing: glide -> flare (wings high, forward) -> fold at touchdown. 0.8 s.
    action = bpy.data.actions.new(name + '_Landing'); action.use_fake_user = True
    rig.animation_data.action = action
    for frame, pose, pitch in ((1, GLIDE, 0), (18, FLARE, 0), (30, blend(FLARE, UP, .5), 0), (40, FLARE, PITCH * .5), (49, FOLDED, PITCH)):
        apply(rig, pose, root_pitch=pitch); key(rig, frame)
    # Perched: folded wings, slow breathing and a head-bob, seamless 3 s loop.
    action = bpy.data.actions.new(name + '_Perched'); action.use_fake_user = True
    rig.animation_data.action = action
    frames = 3 * FPS
    for frame in range(0, frames + 1, 6):
        phase = math.tau * frame / frames
        pose = {'wing': (FOLDED['wing'][0] + 1.5 * math.sin(phase), 0, FOLDED['wing'][2]), 'tip': FOLDED['tip']}
        apply(rig, pose, root_pitch=PITCH + 2.0 * math.sin(2 * phase) + 1.0 * math.sin(3 * phase))
        key(rig, frame + 1)
    # TakeOff: folded -> wings up -> strong downstrokes -> ends mid-flap. 0.6 s.
    action = bpy.data.actions.new(name + '_TakeOff'); action.use_fake_user = True
    rig.animation_data.action = action
    for frame, pose, pitch in ((1, FOLDED, PITCH), (7, UP, PITCH * .5), (14, DOWN, 0), (21, UP, 0), (28, DOWN, 0), (37, GLIDE, 0)):
        apply(rig, pose, root_pitch=pitch); key(rig, frame)
    # Perched support height: lowest deformed vertex in the folded pose.
    rig.animation_data.action = None
    apply(rig, FOLDED, root_pitch=PITCH)
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    lowest = min((o.evaluated_get(deps).matrix_world @ v.co).z for o in bpy.context.scene.objects if o.type == 'MESH'
                 for v in o.evaluated_get(deps).to_mesh().vertices)
    apply(rig, GLIDE)
    rig.animation_data.action = bpy.data.actions[name + '_Gliding']
    bpy.ops.wm.save_as_mainfile(filepath=str(FLIGHT / (name + '.blend')))
    # Export only the new clips; the imported mesh, skeleton and flight clips are unchanged.
    for action in list(bpy.data.actions):
        if not action.name.endswith(('_Landing', '_Perched', '_TakeOff')):
            bpy.data.actions.remove(action)
    rig.animation_data.action = bpy.data.actions[name + '_Perched']
    bpy.ops.object.select_all(action='SELECT')
    fbx = FLIGHT / (name + '_Perch.fbx')
    bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH', 'ARMATURE'},
        add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=False)
    report = {'id': name, 'fbx': str(fbx), 'clips': ['Landing', 'Perched', 'TakeOff'], 'fps': FPS,
              'perch_height_cm': round(-lowest * 100, 1),
              'scope': 'Rotation-only wing fold on a flight-posture mesh; distance-view quality'}
    (FLIGHT / (name + '_Perch.json')).write_text(json.dumps(report, indent=2))
    print('BIRD_PERCH_READY', json.dumps(report))


for bird in sys.argv[sys.argv.index('--') + 1:]:
    author(bird)
