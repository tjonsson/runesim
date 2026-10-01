"""Prepare the NASA Global Hawk (RQ-4) for Unreal: metric scale, +X forward, origin at centre.

blender -b --factory-startup -P Scripts/prepare_global_hawk.py
Source: Art/LivingWorld/Sourced/NASA/GlobalHawk.glb (unchanged). Credit: NASA / Michael D. Carbajal.
Scale reference: NASA operates RQ-4A (published span 35.4 m, length 13.5 m). The model is
proportioned differently (span/length about 2.0 vs 2.6), so it is scaled by span; its length reads ~17.4 m.
"""
import bpy
import json
import math
from mathutils import Matrix, Vector
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / 'Art/LivingWorld/Sourced/NASA/GlobalHawk.glb'
OUT = PROJECT / 'Art/LivingWorld/Prepared/Flight'
SPAN_M = 35.4

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(SOURCE))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects:
    if o.type != 'MESH':
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
body.name = 'GlobalHawk'
points = [body.matrix_world @ v.co for v in body.data.vertices]
lo = Vector([min(p[i] for p in points) for i in range(3)])
hi = Vector([max(p[i] for p in points) for i in range(3)])
span_axis = max(range(2), key=lambda i: hi[i] - lo[i])
scale = SPAN_M / (hi[span_axis] - lo[span_axis])
centre = (lo + hi) / 2
# Nose points to -Y in the source (inspected top view); rotate so the nose faces +X.
rotation = Matrix.Rotation(math.radians(-90), 4, 'Z')
body.data.transform(rotation @ Matrix.Scale(scale, 4) @ Matrix.Translation(-centre) @ body.matrix_world)
body.matrix_world = Matrix.Identity(4)
points = [v.co for v in body.data.vertices]
lo = Vector([min(p[i] for p in points) for i in range(3)])
hi = Vector([max(p[i] for p in points) for i in range(3)])
nose_x = max(points, key=lambda p: p.x)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'GlobalHawk.blend'))
bpy.ops.export_scene.fbx(filepath=str(OUT / 'GlobalHawk.fbx'), use_selection=False, object_types={'MESH'},
    apply_scale_options='FBX_SCALE_UNITS', axis_forward='X', axis_up='Z', path_mode='COPY', embed_textures=True,
    mesh_smooth_type='FACE')
report = {'id': 'GlobalHawk', 'span_m': round(hi.y - lo.y, 2), 'length_m': round(hi.x - lo.x, 2), 'height_m': round(hi.z - lo.z, 2),
          'scale_from_source': scale, 'vertices': len(body.data.vertices), 'materials': [m.name for m in body.data.materials],
          'source': str(SOURCE), 'credit': 'NASA / Michael D. Carbajal', 'license': 'NASA media usage guidelines; no endorsement implied'}
(OUT / 'GlobalHawk.json').write_text(json.dumps(report, indent=2))
print('GLOBALHAWK_READY', json.dumps(report))
