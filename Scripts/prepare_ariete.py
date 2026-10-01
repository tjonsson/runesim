"""Prepare the C1 Ariete MBT (Sketchfab, DustyMojito, Free Standard licence) for Unreal.

blender -b --factory-startup -P Scripts/prepare_ariete.py
Input: Art/LivingWorld/Sourced/Sketchfab/Ariete (git-ignored; the Standard licence allows use in
the project but not redistribution of the raw files). Blender 5.2 has no Collada importer, so
source/model/model.dae is first converted with trimesh+pycollada to source/ariete_converted.glb
(geometry and UVs only; one node per material). This script rebuilds the PBR materials from the
supplied textures, scales to the published 7.59 m hull length (gun excluded) and exports a rigid FBX.
"""
import bpy
import json
from pathlib import Path
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parents[1]
source = root / 'Art/LivingWorld/Sourced/Sketchfab/Ariete'
out = root / 'Art/LivingWorld/Prepared/Ground'
HULL_LENGTH_M = 7.59
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source / 'source/ariete_converted.glb'))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in meshes:
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
for o in list(bpy.context.scene.objects):
    if o.type != 'MESH':
        bpy.data.objects.remove(o, do_unlink=True)
hull = [o.matrix_world @ v.co for o in meshes if o.name.startswith('Hull') for v in o.data.vertices]
every = [v.co for o in meshes for v in o.data.vertices]
scale = HULL_LENGTH_M / (max(p.y for p in hull) - min(p.y for p in hull))
lo = Vector([min(p[i] for p in every) for i in range(3)])
hi = Vector([max(p[i] for p in every) for i in range(3)])
hull_centre_y = (max(p.y for p in hull) + min(p.y for p in hull)) / 2
# Origin: hull centre on the ground plane (lowest track point).
offset = Matrix.Translation((-(lo.x + hi.x) / 2, -hull_centre_y, -lo.z))
textures = source / 'textures'


def texture(tree, name, colour):
    candidates = [p for p in textures.glob(name + '.*')]
    if not candidates:
        return None
    node = tree.nodes.new('ShaderNodeTexImage')
    node.image = bpy.data.images.load(str(candidates[0]))
    node.image.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    return node


for o in meshes:
    o.data.transform(Matrix.Scale(scale, 4) @ offset)
    part = o.name.split('.')[0]
    material = bpy.data.materials.new('M_Ariete_' + part)
    tree = material.node_tree
    bsdf = tree.nodes['Principled BSDF']
    links = tree.links
    albedo = texture(tree, part + '_albedo', True)
    if albedo:
        links.new(albedo.outputs['Color'], bsdf.inputs['Base Color'])
    for suffix, socket in (('metallic', 'Metallic'), ('roughness', 'Roughness')):
        node = texture(tree, f'{part}_{suffix}', False)
        if node:
            links.new(node.outputs['Color'], bsdf.inputs[socket])
    opacity = texture(tree, part + '_opacity', False)
    if opacity:
        links.new(opacity.outputs['Color'], bsdf.inputs['Alpha'])
    normal = texture(tree, part + '_normal', False)
    if normal:
        map_node = tree.nodes.new('ShaderNodeNormalMap')
        links.new(normal.outputs['Color'], map_node.inputs['Color'])
        links.new(map_node.outputs['Normal'], bsdf.inputs['Normal'])
    o.data.materials.clear()
    o.data.materials.append(material)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.join()
tank = bpy.context.view_layer.objects.active
tank.name = 'Ariete'
points = [v.co for v in tank.data.vertices]
lo = Vector([min(p[i] for p in points) for i in range(3)])
hi = Vector([max(p[i] for p in points) for i in range(3)])
bpy.ops.wm.save_as_mainfile(filepath=str(out / 'Ariete.blend'))
bpy.ops.export_scene.fbx(filepath=str(out / 'Ariete.fbx'), use_selection=False, object_types={'MESH'},
    axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True, mesh_smooth_type='FACE')
report = {'id': 'Ariete', 'hull_length_m': HULL_LENGTH_M, 'overall_length_m': round(hi.y - lo.y, 2),
          'width_m': round(hi.x - lo.x, 2), 'height_m': round(hi.z - lo.z, 2), 'scale_from_source': scale,
          'triangles': sum(len(p.vertices) - 2 for p in tank.data.polygons), 'source_forward': '-Y (gun)',
          'source': 'https://sketchfab.com/3d-models/c1-ariete-italian-mbt-c74fbf138e0244ae9160607954009e17',
          'author': 'DustyMojito', 'license': 'Sketchfab Free Standard licence (use in project; no raw redistribution)'}
(out / 'Ariete.json').write_text(json.dumps(report, indent=2))
print('ARIETE_READY', json.dumps(report))
