"""Create editable two-joint wing rigs and seamless flight clips from Tripo birds.

Flight-only profiles deliberately do not use ground/perching clips. Generated
anatomy is an artistic approximation, with recorded reference wingspans.
"""
import bpy
import math
import json
import re
import sys
from pathlib import Path
from mathutils import Vector

PROJECT=Path(__file__).resolve().parents[1]
OUT=PROJECT/'Art/LivingWorld/Prepared/Flight'
OUT.mkdir(parents=True,exist_ok=True)

def smooth(a,b,x):
    t=max(0,min(1,(x-a)/(b-a)))
    return t*t*(3-2*t)

def rig_bird(name,source,span,frequency):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    # Tripo may preserve a banked photographic pose. Align the span between
    # distal wing regions before defining joints, keeping the body rigid.
    xmin=min(p.x for p in points); xmax=max(p.x for p in points)
    tips=[]
    for predicate in (lambda p:p.x<xmin+(xmax-xmin)*.075,lambda p:p.x>xmax-(xmax-xmin)*.075):
        selected=[p for p in points if predicate(p)]
        tips.append(sum(selected,Vector())/len(selected))
    alignment=(tips[1]-tips[0]).normalized().rotation_difference(Vector((1,0,0)))
    points=[alignment@p for p in points]
    lo=Vector([min(p[i] for p in points) for i in range(3)])
    hi=Vector([max(p[i] for p in points) for i in range(3)])
    center=(lo+hi)*.5; factor=span/(hi.x-lo.x)
    for o in meshes:
        matrix=o.matrix_world.copy(); o.parent=None
        for v in o.data.vertices:v.co=(alignment@(matrix@v.co)-center)*factor
        o.matrix_world.identity()
    for o in list(bpy.context.scene.objects):
        if o not in meshes:bpy.data.objects.remove(o,do_unlink=True)
    arm=bpy.data.armatures.new(name+'_Skeleton')
    rig=bpy.data.objects.new(name+'_Rig',arm); bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig; rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    root=arm.edit_bones.new('root'); root.head=(0,0,0); root.tail=(0,span*.08,0)
    for side,sign in [('L',-1),('R',1)]:
        shoulder=arm.edit_bones.new('wing_'+side)
        shoulder.head=(sign*span*.09,0,span*.04)
        shoulder.tail=(sign*span*.27,0,span*.04); shoulder.parent=root
        wrist=arm.edit_bones.new('tip_'+side)
        wrist.head=shoulder.tail; wrist.tail=(sign*span*.47,0,span*.025); wrist.parent=shoulder
    bpy.ops.object.mode_set(mode='OBJECT')
    for o in meshes:
        o.vertex_groups.clear()
        groups={n:o.vertex_groups.new(name=n) for n in arm.bones.keys()}
        for v in o.data.vertices:
            x=abs(v.co.x)/span; side='L' if v.co.x<0 else 'R'
            wing=smooth(.065,.135,x); tip=smooth(.23,.34,x)
            for bone,weight in [('root',1-wing),('wing_'+side,wing*(1-tip)),('tip_'+side,wing*tip)]:
                if weight>0:groups[bone].add([v.index],weight,'REPLACE')
        o.modifiers.new('WingDeformation','ARMATURE').object=rig; o.parent=rig
    textures=OUT/'textures'/name; textures.mkdir(parents=True,exist_ok=True)
    for img in bpy.data.images:
        if img.type=='IMAGE' and len(img.pixels):
            img.filepath_raw=str(textures/(re.sub(r'[^A-Za-z0-9_.-]','_',img.name)+'.png'))
            img.file_format='PNG'; img.save()
    rig.animation_data_create()
    frames=round(60/frequency)
    for clip in ('Flapping','Gliding'):
        action=bpy.data.actions.new(name+'_'+clip); action.use_fake_user=True
        rig.animation_data.action=action
        for frame in range(frames+1):
            phase=math.tau*frame/frames
            for bone in rig.pose.bones:
                bone.rotation_mode='XYZ'; bone.rotation_euler=(0,0,0)
                if bone.name.startswith(('wing_','tip_')):
                    # Local X is perpendicular to the spanwise bone axis.
                    amplitude=math.radians(24 if bone.name.startswith('wing') else 10)
                    bone.rotation_euler.x=amplitude*math.sin(phase-(.4 if bone.name.startswith('tip') else 0)) if clip=='Flapping' else 0
                bone.keyframe_insert('rotation_euler',frame=frame+1)
    rig.animation_data.action=bpy.data.actions[name+'_Gliding']
    scene=bpy.context.scene; scene.render.fps=60; scene.frame_start=1; scene.frame_end=frames+1; scene.frame_set(1)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'.blend')))
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},
        add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z',path_mode='COPY',embed_textures=True)
    report={'id':name,'span_m':span,'frequency_hz':60/frames,'bones':list(arm.bones.keys()),
        'clips':['Flapping','Gliding'],'source':str(source),'fbx':str(OUT/(name+'.fbx')),
        'license':'AI generated with user-authorized Tripo credits; retain service provenance',
        'scope':'Flight rig, no landing/perching; dimensions are representative artistic scaling'}
    (OUT/(name+'.json')).write_text(json.dumps(report,indent=2))
    print('BIRD_FLIGHT_READY',name,span,frames)

for name in sys.argv[sys.argv.index('--')+1:]:
    if name=='DetailedPigeon':rig_bird(name,PROJECT/'Art/LivingWorld/Generated/Pigeon/Pigeon_Tripo.glb',.68,5)
    elif name=='Gull':rig_bird(name,PROJECT/'Art/LivingWorld/Generated/Gull/Gull_Tripo.glb',1.4,2.5)
    elif name=='Crow':rig_bird(name,PROJECT/'Art/LivingWorld/Generated/Crow/Crow_Tripo.glb',.95,4.)
    else:raise ValueError(name)
