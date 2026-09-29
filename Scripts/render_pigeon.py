import bpy
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root / 'Art/LivingWorld/Prepared/Pigeon/Pigeon.blend'), use_scripts=False)
scene = bpy.context.scene
rig = bpy.data.objects['PigeonRig']
center = Vector((0, 0, .06))
bpy.ops.object.camera_add(location=(.65, -.85, .55))
camera = bpy.context.object
camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = .85
scene.camera = camera
for location, energy in [((.4, -.5, .8), 35), ((-.5, -.1, .4), 20), ((0, .5, .7), 25)]:
    bpy.ops.object.light_add(type='AREA', location=location)
    lamp = bpy.context.object
    lamp.data.energy = energy
    lamp.data.size = .8
    lamp.rotation_euler = (center - lamp.location).to_track_quat('-Z', 'Y').to_euler()
scene.world = bpy.data.worlds.new('PigeonReviewWorld')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.15, .17, .2, 1)
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.render.resolution_x = 800
scene.render.resolution_y = 650
scene.render.resolution_percentage = 100
for clip, frame in [('Flapping', 1), ('Flapping', 8), ('Standing_Idle', 1)]:
    rig.animation_data.action = bpy.data.actions['Pigeon_' + clip]
    scene.frame_set(frame)
    scene.render.filepath = str(root / f'Saved/LivingWorld/Pigeon-{clip}-{frame}.png')
    bpy.ops.render.render(write_still=True)
