"""Create M_SmokeSoft (radial falloff, no square sprite edges; depth-faded soft particles) and use it
for every Living World smoke emitter. Editor, PIE stopped.

A new material is built rather than editing M_Smoke in place: clearing a material that loaded Niagara
systems reference trips an engine assertion in UE 5.8.
Opacity = texture.R x particle alpha x radial gradient, then depth fade (150 cm).
"""
import json
import unreal

lib = unreal.EditorAssetLibrary
graph = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
folder = '/Game/LivingWorld/Effects'
texture = unreal.load_asset(folder + '/T_Smoke')
material = unreal.load_asset(folder + '/M_SmokeSoft')
if not material:
    material = tools.create_asset('M_SmokeSoft', folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    sample = graph.create_material_expression(material, unreal.MaterialExpressionTextureSample, -900, 150)
    sample.texture = texture
    color = graph.create_material_expression(material, unreal.MaterialExpressionParticleColor, -900, -150)
    gradient = graph.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -900, 400)
    gradient.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential'))
    opacity = graph.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 150)
    graph.connect_material_expressions(sample, 'R', opacity, 'A')
    graph.connect_material_expressions(color, 'A', opacity, 'B')
    soft = graph.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 250)
    graph.connect_material_expressions(opacity, '', soft, 'A')
    graph.connect_material_expressions(gradient, '', soft, 'B')
    fade = graph.create_material_expression(material, unreal.MaterialExpressionDepthFade, -200, 250)
    fade.set_editor_property('fade_distance_default', 150.0)
    graph.connect_material_expressions(soft, '', fade, 'Opacity')
    graph.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    graph.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)
    graph.recompile_material(material)
    assert lib.save_loaded_asset(material, False)
edit = unreal.LivingEffectAuthoring
updated = []
for name in ('NS_SmokePuff', 'NS_Explosion', 'NS_MissileTrail', 'NS_AirBurst'):
    system = unreal.load_asset(folder + '/' + name)
    error = edit.set_renderer(system, 'OmnidirectionalBurst', 0, json.dumps({'Material': material.get_path_name()}))
    assert not error, (name, error)
    assert lib.save_loaded_asset(system, False)
    updated.append(name)
print('SMOKE_SOFTENED', updated)
