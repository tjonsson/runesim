import bpy
import sys
import math
from pathlib import Path
from mathutils import Vector
name=sys.argv[sys.argv.index('--')+1]
project=Path(__file__).resolve().parents[1]
source=project/'Art/LivingWorld/Prepared'/f'{name}.blend'
if '--flight' in sys.argv: source=project/'Art/LivingWorld/Prepared/Flight'/f'{name}.blend'
if '--source' in sys.argv: source=Path(sys.argv[sys.argv.index('--source')+1])
bpy.ops.wm.open_mainfile(filepath=str(source))
if '--clip' in sys.argv:
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    rig.animation_data.action=bpy.data.actions[name+'_'+sys.argv[sys.argv.index('--clip')+1]]
if '--frame' in sys.argv: bpy.context.scene.frame_set(int(sys.argv[sys.argv.index('--frame')+1]))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
points=[o.matrix_world@Vector(p) for o in meshes for p in o.bound_box]
lo=Vector([min(p[i] for p in points) for i in range(3)])
hi=Vector([max(p[i] for p in points) for i in range(3)])
center=(lo+hi)*.5
size=max(hi-lo)
bpy.ops.object.camera_add(location=center+Vector((1.2,-1.6,1.1))*size)
camera=bpy.context.object; camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'; camera.data.ortho_scale=size*1.5
bpy.context.scene.camera=camera
for offset,power in [((1,-1,2),100),((-1,-.5,1),70),((0,1,1),120)]:
    bpy.ops.object.light_add(type='AREA',location=center+Vector(offset)*size)
    light=bpy.context.object; light.data.energy=power*size*size; light.data.shape='DISK'; light.data.size=size*2
    light.rotation_euler=(center-light.location).to_track_quat('-Z','Y').to_euler()
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=16
scene.render.resolution_x=900; scene.render.resolution_y=700; scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('InspectionWorld'); scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.14,.18,1)
suffix='flight' if '--flight' in sys.argv else 'textured'
scene.render.filepath=str(project/'Saved/LivingWorld'/f'{name}-{suffix}.png')
bpy.ops.render.render(write_still=True)
print('PARTS',[(o.name,[round(v,4) for v in (o.matrix_world@sum((Vector(p) for p in o.bound_box),Vector())/8)],list(o.dimensions)) for o in meshes])
