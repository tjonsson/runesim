"""Articulate the four visible wheels of the existing Tripo utility jeep.

The generated shape is a generic vehicle, not a certified military subtype.
Wheel segmentation and a 4.1 m overall length are project art decisions.
"""
import bpy
import math
import json
from pathlib import Path
from mathutils import Vector

root=Path(__file__).resolve().parents[1]
out=root/'Art/LivingWorld/Prepared/Ground';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(root/'Art/LivingWorld/Generated/Jeep/Jeep_Tripo.glb'))
mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
matrix=mesh.matrix_world.copy()
for v in mesh.data.vertices:v.co=matrix@v.co
mesh.matrix_world.identity()
scale=4.1/(max(v.co.y for v in mesh.data.vertices)-min(v.co.y for v in mesh.data.vertices))
centers={side+end:Vector((sign*.208,y,.122)) for side,sign in [('L',-1),('R',1)] for end,y in [('F',-.291),('R',.320)]}
arm=bpy.data.armatures.new('JeepSkeleton');rig=bpy.data.objects.new('JeepRig',arm);bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig;rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
base=arm.edit_bones.new('root');base.head=(0,0,0);base.tail=(0,.3,0)
for name,center in centers.items():
    bone=arm.edit_bones.new('wheel_'+name);bone.head=center*scale
    bone.tail=bone.head+Vector((.2,0,0));bone.parent=base
bpy.ops.object.mode_set(mode='OBJECT')
groups={name:mesh.vertex_groups.new(name=name) for name in arm.bones.keys()}
counts={name:0 for name in groups}
for v in mesh.data.vertices:
    bone='root'
    for name,c in centers.items():
        if v.co.x*c.x>0 and abs(v.co.x)>.151 and math.hypot(v.co.y-c.y,v.co.z-c.z)<.124:
            bone='wheel_'+name;break
    groups[bone].add([v.index],1,'REPLACE');counts[bone]+=1
    v.co*=scale
mesh.modifiers.new('WheelArticulation','ARMATURE').object=rig;mesh.parent=rig
rig.animation_data_create()
action=bpy.data.actions.new('Jeep_Driving');rig.animation_data.action=action
for frame in range(61):
    for bone in rig.pose.bones:
        bone.rotation_mode='XYZ';bone.rotation_euler=(0,0,0)
        # Local Y follows the axle; one revolution per second is scaled at runtime.
        if bone.name.startswith('wheel_'):bone.rotation_euler.y=-math.tau*frame/60
        bone.keyframe_insert('rotation_euler',frame=frame+1)
textures=out/'textures/Jeep';textures.mkdir(parents=True,exist_ok=True)
for i,img in enumerate(bpy.data.images):
    if img.type=='IMAGE' and len(img.pixels):
        img.filepath_raw=str(textures/f'texture_{i}.png');img.file_format='PNG';img.save()
scene=bpy.context.scene;scene.render.fps=60;scene.frame_start=1;scene.frame_end=61;scene.frame_set(1)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Jeep.blend'))
bpy.ops.export_scene.fbx(filepath=str(out/'Jeep.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,
    bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,
    axis_forward='-Y',axis_up='Z',path_mode='COPY',embed_textures=True)
(out/'Jeep.json').write_text(json.dumps({'source_task':'27449fe4-f4cd-441b-8edc-ebc01c6b3137','length_m':4.1,
    'wheel_radius_m':.122*scale,'bone_vertices':counts,'source_forward':'-Y','license':'Existing user Tripo generated asset; no new credits consumed',
    'scope':'Generic textured utility vehicle with approximate rigid wheel segmentation; not a verified NATO subtype'},indent=2))
print('JEEP_RIG_READY',counts)
