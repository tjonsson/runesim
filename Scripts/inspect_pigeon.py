"""Inspect the downloaded original without executing embedded Blender scripts."""
import bpy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root / 'Art/LivingWorld/Sourced/Pigeon/bird-original.blend'), use_scripts=False)
report = {
    'objects': [{'name': o.name, 'type': o.type, 'dimensions': list(o.dimensions),
                 'rotation': list(o.rotation_euler), 'scale': list(o.scale),
                 'parent': o.parent.name if o.parent else None,
                 'materials': [s.name for s in o.material_slots],
                 'bones': [b.name for b in o.data.bones] if o.type == 'ARMATURE' else [],
                 'nla': [{'name': t.name, 'strips': [s.name for s in t.strips]} for t in o.animation_data.nla_tracks] if o.animation_data else []}
                for o in bpy.data.objects],
    'actions': [{'name': a.name, 'range': list(a.frame_range)} for a in bpy.data.actions],
    'images': [{'name': i.name, 'path': i.filepath, 'packed': bool(i.packed_file)} for i in bpy.data.images],
    'texts': [{'name': t.name, 'text': t.as_string()[:6000]} for t in bpy.data.texts],
    'fps': bpy.context.scene.render.fps,
}
path = root / 'Saved/LivingWorld/pigeon-inspection.json'
path.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
