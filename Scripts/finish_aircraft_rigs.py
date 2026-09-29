"""Build reviewed flight variants without overwriting the supplied/prepared originals.

Blender background: --python Scripts/finish_aircraft_rigs.py -- Mavic3 UH1 AH1W
All distances are metres. Animated rotor rates are visual sampling rates, not RPM telemetry.
"""
import bpy
import json
import math
import re
import shutil
import sys
from pathlib import Path
from mathutils import Vector, Matrix

PROJECT = Path(__file__).resolve().parents[1]
PREPARED = PROJECT / 'Art/LivingWorld/Prepared'
OUT = PREPARED / 'Flight'
OUT.mkdir(parents=True, exist_ok=True)

def bounds(parts):
    points = [o.matrix_world @ Vector(v) for o in parts for v in o.bound_box]
    return (Vector([min(p[i] for p in points) for i in range(3)]),
            Vector([max(p[i] for p in points) for i in range(3)]))

def finish(name):
    bpy.ops.wm.open_mainfile(filepath=str(PREPARED / (name+'.blend')))
    scene = bpy.context.scene
    scene.frame_set(1)
    meshes = [o for o in scene.objects if o.type == 'MESH']
    rig = next(o for o in scene.objects if o.type == 'ARMATURE')
    rig.animation_data_clear()
    for bone in rig.pose.bones:
        bone.rotation_mode='XYZ'; bone.rotation_euler=(0,0,0)
    groups = {}
    if name == 'F4':
        # Flight-only mesh variant: stow wheels/struts and close the separate
        # gear doors around their upper edge. The complete ground source survives.
        for obj in list(meshes):
            if re.match(r'roue|axe[AGD][BH]|articule|catap',obj.name):
                meshes.remove(obj); bpy.data.objects.remove(obj,do_unlink=True)
            elif obj.name.startswith('porte'):
                a,b=bounds([obj]); pivot=Vector(((a.x+b.x)*.5,(a.y+b.y)*.5,b.z))
                angle=math.pi/2*(-1 if pivot.y<0 else 1)
                rotation=Matrix.Rotation(angle,4,'X')
                if abs(pivot.y)<.1: rotation=Matrix.Rotation(math.pi/2,4,'Y')
                for v in obj.data.vertices: v.co=pivot+rotation.to_3x3()@(v.co-pivot)
    if name == 'UH1':
        # Source inspection: numbered blade1..4 are the MAIN blades; unnumbered
        # blade / blade.001 are the TAIL blades. The old name heuristic reversed them.
        groups['main_rotor'] = ([o for o in meshes if re.match(r'blade[1-4]|axe',o.name)], Vector((-2.0121,0,1.6859)), Vector((0,0,1)), 3)
        groups['tail_rotor'] = ([o for o in meshes if re.match(r'blade[._]|rotortail',o.name)], Vector((7.0191,-.4719,1.649)), Vector((0,1,0)), 7)
    elif name == 'Mavic3':
        for i in range(1,5):
            parts = [o for o in meshes if o.name.startswith('桨叶'+str(i)+'_')]
            hub = next(o for o in parts if '_01.' in o.name)
            a,b=bounds([hub]); pivot=(a+b)*.5
            groups['propeller_'+str(i)]=(parts,pivot,Vector((0,0,1)),4 if i in (1,2) else -4)
    elif name == 'AH1W':
        def part(number): return next(o for o in meshes if o.name.startswith('mesh'+str(number)+'_'))
        groups['main_rotor']=([part(i) for i in (21,23,39,40,41)],Vector((-1.5518,0,1.6958)),Vector((0,0,1)),3)
        groups['tail_rotor']=([part(i) for i in (26,27,28,29,30,31,32,33,34,36,37)],Vector((7.1443,.5108,.8441)),Vector((0,1,0)),7)
        # USD's OmniPBR/MDL shaders are unsupported by Blender's importer. Rebuild
        # their actual recorded texture bindings, rather than inventing camouflage.
        bindings=json.loads((PREPARED/'AH1W-material-bindings.json').read_text())
        textures=OUT/'textures'/'AH1W'; textures.mkdir(parents=True,exist_ok=True)
        source=Path('C:/Users/tommy/Desktop/synthetic_image_gen/assets/models/helicopter/supercopra')
        for material in bpy.data.materials:
            data=bindings[material.name]
            material.use_nodes=True
            nodes=material.node_tree.nodes; links=material.node_tree.links
            nodes.clear()
            shader=nodes.new('ShaderNodeBsdfPrincipled')
            output=nodes.new('ShaderNodeOutputMaterial')
            links.new(shader.outputs['BSDF'],output.inputs['Surface'])
            shader.inputs['Base Color'].default_value=(*data['color'],1)
            shader.inputs['Roughness'].default_value=.55
            if data.get('texture'):
                src=source/data['texture']; dst=textures/src.name
                shutil.copy2(src,dst)
                node=nodes.new('ShaderNodeTexImage'); node.image=bpy.data.images.load(str(dst),check_existing=True)
                links.new(node.outputs['Color'],shader.inputs['Base Color'])
            if material.name.startswith('Glass'):
                shader.inputs['Roughness'].default_value=.14
                shader.inputs['Metallic'].default_value=.25
    if groups:
        bpy.context.view_layer.objects.active=rig
        bpy.ops.object.mode_set(mode='EDIT')
        for bone in list(rig.data.edit_bones):
            if bone.name!='root': rig.data.edit_bones.remove(bone)
        for group,(_,pivot,axis,_) in groups.items():
            bone=rig.data.edit_bones.new(group); bone.head=pivot
            bone.tail=pivot+axis*.05; bone.parent=rig.data.edit_bones['root']
        bpy.ops.object.mode_set(mode='OBJECT')
        for obj in meshes:
            group=next((k for k,(parts,*_) in groups.items() if obj in parts),'root')
            obj.vertex_groups.clear()
            obj.vertex_groups.new(name=group).add(list(range(len(obj.data.vertices))),1.,'REPLACE')
        for group,(_,_,_,turns) in groups.items():
            bone=rig.pose.bones[group]; bone.rotation_mode='XYZ'
            for frame in range(1,62):
                bone.rotation_euler.y=(frame-1)/60*math.tau*turns
                bone.keyframe_insert('rotation_euler',frame=frame)
    else:
        rig.pose.bones['root'].keyframe_insert('rotation_euler',frame=1)
        rig.pose.bones['root'].keyframe_insert('rotation_euler',frame=61)
    scene.render.fps=60; scene.frame_start=1; scene.frame_end=61
    action=rig.animation_data.action; action.name=name+'_Cruise'
    for layer in action.layers:
        for strip in layer.strips:
            for slot in action.slots:
                bag=strip.channelbag(slot)
                if bag:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:key.interpolation='LINEAR'
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'.blend')))
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},
        add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z',path_mode='COPY',embed_textures=True)
    a,b=bounds(meshes)
    report={'id':name,'dimensions_m':list(b-a),'bones':list(rig.data.bones.keys()),
        'triangles':sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes),
        'parts':len(meshes),'source_forward':'+X' if name=='Mavic3' else '-X',
        'rotors':{k:{'pivot_m':list(v[1]),'parts':[o.name for o in v[0]]} for k,v in groups.items()},
        'license':'User-supplied; private project use; redistribution rights unverified',
        'fbx':str(OUT/(name+'.fbx'))}
    (OUT/(name+'.json')).write_text(json.dumps(report,indent=2))
    print('FLIGHT_RIG',name,report['bones'],report['triangles'])

for name in sys.argv[sys.argv.index('--')+1:]: finish(name)
