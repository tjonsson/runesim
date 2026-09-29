"""Render three poses for review from the prepared humanoid library."""
import bpy
import sys
from pathlib import Path
from mathutils import Vector
root = Path(__file__).resolve().parents[1]
kind = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.open_mainfile(filepath=str(root / f'Art/LivingWorld/Prepared/{kind}/{kind}.blend'), use_scripts=False)
scene = bpy.context.scene
rig = next(o for o in scene.objects if o.type == 'ARMATURE')
center = Vector((0, 0, .5))
bpy.ops.object.camera_add(location=(1.0, -1.8, .85))
camera = bpy.context.object
camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 1.25
scene.camera = camera
for pos, power in [((1,-1,2),100),((-1,-.5,1.5),70),((0,1,2),80)]:
    bpy.ops.object.light_add(type='AREA', location=pos)
    light = bpy.context.object
    light.data.energy = power
    light.data.size = 1.5
    light.rotation_euler = (center-light.location).to_track_quat('-Z','Y').to_euler()
scene.world = bpy.data.worlds.new('ReviewWorld')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.1,.12,.15,1)
scene.render.engine = 'CYCLES'
scene.cycles.samples = 16
scene.render.resolution_x = 640
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
for name, frame in [('Walk',15),('Run',10),('Idle',1)]:
    action = bpy.data.actions[kind+'_'+name]
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    scene.frame_set(frame)
    scene.render.filepath = str(root / f'Saved/LivingWorld/{kind}-{name}.png')
    bpy.ops.render.render(write_still=True)
