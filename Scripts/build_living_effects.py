"""Author bounded CPU Niagara effects using inspected UE 5.8 template modules.

The editor adapter uses Epic's experimental external-edit API only at authoring
time. Saved Niagara packages run through the ordinary engine at runtime.
"""
import unreal
import json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
lib=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/LivingWorld/Effects';edit=unreal.LivingEffectAuthoring

texture=unreal.load_asset(folder+'/T_Smoke') or lib.duplicate_asset('/Engine/Tutorial/SubEditors/TutorialAssets/T_soft_smoke',folder+'/T_Smoke')
material=unreal.load_asset(folder+'/M_Smoke')
if not material:
    material=tools.create_asset('M_Smoke',folder,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided',True)
    graph=unreal.MaterialEditingLibrary
    sample=graph.create_material_expression(material,unreal.MaterialExpressionTextureSample,-500,150);sample.texture=texture
    color=graph.create_material_expression(material,unreal.MaterialExpressionParticleColor,-500,-100)
    opacity=graph.create_material_expression(material,unreal.MaterialExpressionMultiply,-200,150)
    graph.connect_material_expressions(sample,'R',opacity,'A');graph.connect_material_expressions(color,'A',opacity,'B')
    graph.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    graph.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    graph.recompile_material(material);lib.save_loaded_asset(material)

def put(system,emitter,stack,module,key,kind,value):
    values=list(value) if isinstance(value,(tuple,list)) else [value]
    result=edit.set_input(system,emitter,stack,module,key,kind,unreal.Vector4(*(values+[0]*(4-len(values)))))
    assert not result,(system.get_name(),emitter,module,key,result)
def init(system,emitter,**values):
    for key,value in values.items():put(system,emitter,'ParticleSpawnScript','InitializeParticle',key.replace('_',' '),'color' if isinstance(value,tuple) else 'float',value)
def spawn(system,emitter,count):put(system,emitter,'EmitterUpdateScript','SpawnBurst_Instantaneous','Spawn Count','int',count)
def clone(name,source):return unreal.load_asset(folder+'/'+name) or lib.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/'+source,folder+'/'+name)

report=[]
for name in ('NS_SmokePuff','NS_Explosion'):
    system=clone(name,'SimpleExplosion');smoke='OmnidirectionalBurst'
    spawn(system,smoke,40)
    init(system,smoke,Lifetime_Min=4.,Lifetime_Max=6.,Uniform_Sprite_Size_Min=120.,Uniform_Sprite_Size_Max=220.,Color=(.08,.09,.10,.55))
    put(system,smoke,'ParticleSpawnScript','ShapeLocation','Sphere Radius','float',75.)
    put(system,smoke,'ParticleSpawnScript','AddVelocity','Velocity Speed','float',110.)
    put(system,smoke,'ParticleUpdateScript','GravityForce','Gravity','vector',(0,0,70))
    put(system,smoke,'ParticleUpdateScript','Drag','Drag','float',1.5)
    error=edit.set_renderer(system,smoke,0,json.dumps({'Material':material.get_path_name()}));assert not error,error
    flash='SimpleSpriteBurst';debris='UpwardMeshBurst'
    spawn(system,flash,0 if name=='NS_SmokePuff' else 5)
    spawn(system,debris,0 if name=='NS_SmokePuff' else 14)
    if name=='NS_Explosion':
        init(system,flash,Lifetime=.35,Uniform_Sprite_Size_Min=230.,Uniform_Sprite_Size_Max=330.,Color=(12.,2.2,.25,1.))
        init(system,debris,Lifetime_Min=.7,Lifetime_Max=1.5,Mesh_Uniform_Scale_Min=.035,Mesh_Uniform_Scale_Max=.09,Color=(.06,.055,.05,1.))
    lib.set_metadata_tag(system,'Source','Epic bundled SimpleExplosion; project-authored smoke, flash and debris tuning')
    lib.save_loaded_asset(system,False);report.append(system.get_path_name())

system=clone('NS_ElectricalSparks','DirectionalBurst')
spawn(system,'DirectionalBurst',45)
init(system,'DirectionalBurst',Lifetime_Min=.15,Lifetime_Max=.4,Color=(1.,3.,9.,1.))
put(system,'DirectionalBurst','ParticleSpawnScript','AddVelocityInCone','Velocity Strength','float',900.)
put(system,'DirectionalBurst','ParticleSpawnScript','AddVelocityInCone','Cone Angle','float',65.)
put(system,'DirectionalBurst','ParticleUpdateScript','GravityForce','Gravity','vector',(0,0,-300))
init(system,'LocationBasedRibbon',Lifetime=.14,Color=(.3,1.8,7.,1.))
lib.set_metadata_tag(system,'Source','Epic bundled DirectionalBurst; short blue electrical spark trails')
lib.save_loaded_asset(system,False);report.append(system.get_path_name())
(Path(unreal.Paths.project_saved_dir())/'LivingWorld/effects-assets.json').write_text(json.dumps(report,indent=2))
print('LIVING_EFFECTS_AUTHORED',report)
