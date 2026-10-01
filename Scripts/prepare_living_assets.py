"""Blender batch prep: preserve originals, normalize units, add rigid articulation rigs.

Run: blender --background --factory-startup --python Scripts/prepare_living_assets.py
Source identity/rights and ambiguous control surfaces remain explicit review gates.
"""
import bpy
import json
import math
import zipfile
from pathlib import Path
from mathutils import Vector, Matrix

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = Path('C:/Users/tommy/Desktop/synthetic_image_gen/assets/models')
OUT = PROJECT / 'Art/LivingWorld/Prepared'
OUT.mkdir(parents=True, exist_ok=True)

def bounds(meshes):
    points = [o.matrix_world @ Vector(p) for o in meshes for p in o.bound_box]
    lo = Vector([min(p[i] for p in points) for i in range(3)])
    hi = Vector([max(p[i] for p in points) for i in range(3)])
    return lo, hi

def prepare(path, name, scale, rotate_z=0):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if path.suffix.lower() == '.fbx': bpy.ops.import_scene.fbx(filepath=str(path))
    elif path.suffix.lower() == '.glb': bpy.ops.import_scene.gltf(filepath=str(path))
    else: bpy.ops.wm.usd_import(filepath=str(path))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    # Bake source hierarchy before deleting empties. No auto-weighting on rigid mechanical parts.
    for obj in meshes:
        matrix = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = Matrix.Rotation(math.radians(rotate_z), 4, 'Z') @ matrix
    lo, hi = bounds(meshes)
    center = (lo + hi) * 0.5
    for obj in meshes:
        obj.location -= center
        obj.location *= scale
        obj.scale *= scale
    for obj in list(bpy.context.scene.objects):
        if obj not in meshes: bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # A root skeleton preserves rigid meshes. Only unambiguous named blades get animation.
    arm = bpy.data.armatures.new(name + '_Skeleton')
    rig = bpy.data.objects.new(name + '_Rig', arm)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    root = arm.edit_bones.new('root')
    root.head = (0, 0, 0); root.tail = (0, 0, 0.1)
    groups = {'root': []}
    # Huey separates rotor blade geometry; derive shared pivots from bounding boxes.
    if name == 'UH1':
        for group, selector, axis in [('main_rotor', lambda n:'blade' in n and not any(c.isdigit() for c in n.split('_')[0].split('.')[0]), 'Z'),
                                       ('tail_rotor', lambda n:'rotortail' in n, 'Y')]:
            parts = [o for o in meshes if selector(o.name.lower())]
            if parts:
                a,b = bounds(parts); pivot = (a+b)*0.5
                bone = arm.edit_bones.new(group); bone.head = pivot
                bone.tail = pivot + (Vector((0,0,.15)) if axis=='Z' else Vector((0,.15,0)))
                bone.parent = root; groups[group] = parts
    bpy.ops.object.mode_set(mode='OBJECT')
    assigned = {o for name_,parts in groups.items() if name_!='root' for o in parts}
    groups['root'] = [o for o in meshes if o not in assigned]
    for group, parts in groups.items():
        for obj in parts:
            obj.vertex_groups.clear()
            obj.vertex_groups.new(name=group).add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')
            obj.modifiers.new('RigidArmature', 'ARMATURE').object = rig
            obj.parent = rig
    bpy.context.scene.render.fps = 30
    bpy.context.scene.frame_start = 1; bpy.context.scene.frame_end = 31
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0,0,0)
        bone.keyframe_insert('rotation_euler', frame=1)
        # Bone local Y is its longitudinal axis.
        if bone.name != 'root': bone.rotation_euler.y = 2*math.pi
        bone.keyframe_insert('rotation_euler', frame=31)
    if rig.animation_data and rig.animation_data.action:
        rig.animation_data.action.name = name + '_Cruise'
        for slot in rig.animation_data.action.slots:
            for layer in rig.animation_data.action.layers:
                for strip in layer.strips:
                    bag = strip.channelbag(slot)
                    if bag:
                        for curve in bag.fcurves:
                            for key in curve.keyframe_points: key.interpolation = 'LINEAR'
    bpy.context.scene.frame_set(1)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / (name + '.blend')))
    bpy.ops.export_scene.fbx(filepath=str(OUT / (name+'.fbx')), use_selection=True, object_types={'MESH','ARMATURE'},
        add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
    lo,hi = bounds(meshes)
    return {'id':name,'source':str(path),'dimensions_m':list(hi-lo),'bones':list(groups),
            'status':'rigged draft; visual, pivots, scale and license review required',
            'fbx':str(OUT/(name+'.fbx')), 'blend':str(OUT/(name+'.blend'))}

items = [
    ('drone/DJI_Mavic_3_Cine.glb','Mavic3',0.01,-90),
    ('drone/MQ27_Dragonfire.glb','MQ27',0.01,-90),
    ('helicopter/Bell_UH1_Iroquois_Huey.glb','UH1',1.0,0),
    ('plane/F15SMT_AceCombat3.glb','F15SMT',0.05,-90),
    ('plane/McDonnell_Douglas_F4_Phantom.glb','F4',1.0,0),
    ('helicopter/supercopra/AH-1W Supercobra Helicopter.usd','AH1W',1.0,-90),
    # Geranium-2 removed 2026-10-01: replaced by the supplied Shahed-136 (Scripts/prepare_shahed.py).
]
report = []
for rel,name,scale,rotation in items:
    try: report.append(prepare(SOURCE/rel,name,scale,rotation))
    except Exception as e: report.append({'id':name,'error':str(e)})
with zipfile.ZipFile('C:/Users/tommy/Downloads/Orqa+MRM1-5.zip') as z:
    staging = PROJECT/'Saved/LivingWorld/OrqaSource'
    staging.mkdir(parents=True,exist_ok=True)
    for entry in z.infolist():
        target=(staging/entry.filename).resolve()
        if not target.is_relative_to(staging.resolve()): raise ValueError('Unsafe archive path')
        z.extract(entry,staging)
    path=next(staging.glob('*.fbx'))
    try: report.append(prepare(path,'Orqa',1.0,0))
    except Exception as e: report.append({'id':'Orqa','error':str(e)})
(OUT/'manifest.json').write_text(json.dumps(report,indent=2))
print('PREPARED_MANIFEST',str(OUT/'manifest.json'))
