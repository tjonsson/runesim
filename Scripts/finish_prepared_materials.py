"""Resolve embedded images to portable files and repair the supplied Orqa material links."""
import bpy
import math
import re
import json
from pathlib import Path
from mathutils import Vector
project=Path(__file__).resolve().parents[1]
out=project/'Art/LivingWorld/Prepared'
report_path=out/'material-and-rig-report.json'
report=json.loads(report_path.read_text()) if report_path.exists() else []
for blend in out.glob('*.blend'):
    bpy.ops.wm.open_mainfile(filepath=str(blend))
    name=blend.stem
    textures=out/'textures'/name; textures.mkdir(parents=True,exist_ok=True)
    if name=='Orqa' and not any('propeller_1' in a.bones for a in bpy.data.armatures):
        source=next((project/'Saved/LivingWorld/OrqaSource').glob('*.fbm'))
        for obj in [o for o in bpy.context.scene.objects if o.type=='MESH']:
            part=re.search(r'tripo_part_(\d+)',obj.name)
            if not part: continue
            for material in obj.data.materials:
                if not material: continue
                material.use_nodes=True
                nodes=material.node_tree.nodes; links=material.node_tree.links
                shader=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
                for suffix,slot in [('basecolor','Base Color'),('normal','Normal')]:
                    file=source/f'Orqa_MRM1-5_tripo_part_{part.group(1)}_{suffix}.JPEG'
                    if not file.exists(): continue
                    tex=nodes.new('ShaderNodeTexImage'); tex.image=bpy.data.images.load(str(file),check_existing=True)
                    if suffix=='normal':
                        tex.image.colorspace_settings.name='Non-Color'
                        normal=nodes.new('ShaderNodeNormalMap'); links.new(tex.outputs['Color'],normal.inputs['Color'])
                        links.new(normal.outputs['Normal'],shader.inputs[slot])
                    else: links.new(tex.outputs['Color'],shader.inputs[slot])
        rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
        # Source parts identified in the inspection render; 5-inch propeller diameter gives scale.
        motor=Vector((.2993,.2602,.0087))
        blade=bpy.data.objects['tripo_part_10']
        radius=max((Vector((v.co.x,v.co.y,0))-Vector((motor.x,motor.y,0))).length for v in blade.data.vertices)
        scale=.127/(radius*2)
        for obj in bpy.context.scene.objects:
            if obj.type=='MESH':
                for vertex in obj.data.vertices: vertex.co*=scale
        bpy.context.view_layer.objects.active=rig
        bpy.ops.object.mode_set(mode='EDIT')
        for bone in rig.data.edit_bones: bone.head*=scale; bone.tail*=scale
        for i,(part,pivot) in enumerate([
            ('tripo_part_10',(.2993,.2602,.0087)),('tripo_part_18',(-.2993,-.2602,.0087)),
            ('tripo_part_2',(-.2993,.2602,.0087)),('tripo_part_6',(.2993,-.2602,.0087))]):
            bone=rig.data.edit_bones.new('propeller_'+str(i+1)); bone.head=Vector(pivot)*scale
            bone.tail=bone.head+Vector((0,0,.025)); bone.parent=rig.data.edit_bones['root']
        bpy.ops.object.mode_set(mode='OBJECT')
        for i,part in enumerate(['tripo_part_10','tripo_part_18','tripo_part_2','tripo_part_6']):
            obj=bpy.data.objects[part]
            indices=[v.index for v in obj.data.vertices if part in ['tripo_part_10','tripo_part_18'] or v.co.z>.01*scale]
            obj.vertex_groups['root'].remove(indices)
            group=obj.vertex_groups.new(name='propeller_'+str(i+1)); group.add(indices,1.,'REPLACE')
            bone=rig.pose.bones[group.name]; bone.rotation_mode='XYZ'
            for frame in range(1,32):
                bone.rotation_euler.y=(frame-1)*2*math.pi/15 * (-1 if i%2 else 1)
                bone.keyframe_insert('rotation_euler',frame=frame)
        report.append({'asset':'Orqa','scale_factor':scale,'propeller_diameter_m':.127,
                       'source':'https://orqafpv.com/products/mrm1-5',
                       'status':'Rotor weights/pivots are a draft inferred from generated parts; inspect in motion'})
    for image in list(bpy.data.images):
        if image.type!='IMAGE': continue
        # Blender lazily loads image buffers after opening a blend file.
        if not len(image.pixels):
            print('MISSING_TEXTURE',name,image.name,image.filepath)
            continue
        safe=re.sub(r'[^A-Za-z0-9_.-]','_',image.name)
        image.filepath_raw=str(textures/(safe+'.png')); image.file_format='PNG'
        image.save()
    # FBX cannot translate glTF's color-factor Mix nodes. Connect the upstream
    # base-color image directly; leave the source GLB untouched for full fidelity.
    for material in bpy.data.materials:
        if not material.use_nodes: continue
        nodes=material.node_tree.nodes; links=material.node_tree.links
        for shader in [n for n in nodes if n.type=='BSDF_PRINCIPLED']:
            socket=shader.inputs['Base Color']
            if socket.is_linked and socket.links[0].from_node.type=='MIX':
                mix=socket.links[0].from_node
                images=[l.from_node for s in mix.inputs for l in s.links if l.from_node.type=='TEX_IMAGE']
                if images: links.new(images[0].outputs['Color'],socket)
    bpy.context.scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')), use_selection=True, object_types={'MESH','ARMATURE'},
        add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        axis_forward='-Y',axis_up='Z',path_mode='COPY',embed_textures=True)
report_path.write_text(json.dumps(report,indent=2))
