"""Run with Blender --background --python; read-only source inspection."""
import bpy
import json
import zipfile
from pathlib import Path
from mathutils import Vector

root = Path('C:/Users/tommy/Desktop/synthetic_image_gen/assets/models')
report = []
for path in root.rglob('*'):
    if path.suffix.lower() not in {'.glb', '.usd', '.usdc'}:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        if path.suffix == '.glb':
            bpy.ops.import_scene.gltf(filepath=str(path))
        else:
            bpy.ops.wm.usd_import(filepath=str(path))
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        points = [o.matrix_world @ Vector(p) for o in meshes for p in o.bound_box]
        bounds = [[min(p[i] for p in points), max(p[i] for p in points)] for i in range(3)] if points else []
        report.append({'source':str(path), 'bounds':bounds, 'objects':[{'name':o.name, 'vertices':len(o.data.vertices), 'dimensions':list(o.dimensions)} for o in meshes], 'armatures':[o.name for o in bpy.data.objects if o.type=='ARMATURE']})
    except Exception as e:
        report.append({'source':str(path), 'error':str(e)})
archive = Path('C:/Users/tommy/Downloads/Orqa+MRM1-5.zip')
with zipfile.ZipFile(archive) as z:
    report.append({'archive':str(archive), 'entries':z.namelist()})
out = Path(__file__).resolve().parents[1] / 'Saved/LivingWorld/source-inspection.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
print('ASSET_REPORT', str(out))
