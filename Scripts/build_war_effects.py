"""Author the battlefield effect set used by USimWarEffects (editor, PIE stopped).

Sources: the Rook & Bolt coastal and vehicle-fluid effects under /Game/LivingWorld/Effects/RookAndBolt
(project-owned; the fluid fire derives from Epic's NiagaraFluids Grid3D_Gas_Fire template). Those were
tuned for a close top-down camera (sprites of centimetres to 1.5 m, culled beyond 85 m). The war set is
resized for tripod and sensor views at up to several kilometres, and camera-distance culling is off.
Values are for Scale 1 (a 155 mm-class ground burst); the runtime multiplies by effect scale.
"""
import json
import unreal

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
lib = unreal.EditorAssetLibrary
edit = unreal.LivingEffectAuthoring
SOURCE = '/Game/LivingWorld/Effects/RookAndBolt/'
DEST = '/Game/LivingWorld/Effects/War/'
report = {}


def derive(name, source):
    path = DEST + name
    # Every tuned value below is set explicitly, so reusing an earlier derivative is equivalent.
    return unreal.load_asset(path) or lib.duplicate_asset(SOURCE + source, path)


def put(system, emitter, stack, module, key, kind, value):
    values = list(value) if isinstance(value, (tuple, list)) else [value]
    result = edit.set_input(system, emitter, stack, module, key, kind, unreal.Vector4(*(values + [0] * (4 - len(values)))))
    assert not result, (system.get_name(), emitter, module, key, result)


def burst(system, emitter, count, life, size, colour, speed=None, radius=None, gravity=None, drag=None, single_lifetime=False):
    put(system, emitter, 'EmitterUpdateScript', 'SpawnBurst_Instantaneous', 'Spawn Count', 'int', count)
    if single_lifetime:
        put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', 'Lifetime', 'float', life[1])
    else:
        put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', 'Lifetime Min', 'float', life[0])
        put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', 'Lifetime Max', 'float', life[1])
    put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', 'Uniform Sprite Size Min', 'float', size[0])
    put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', 'Uniform Sprite Size Max', 'float', size[1])
    put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', 'Color', 'color', colour)
    if radius is not None:
        put(system, emitter, 'ParticleSpawnScript', 'ShapeLocation', 'Sphere Radius', 'float', radius)
    if speed is not None:
        put(system, emitter, 'ParticleSpawnScript', 'AddVelocity', 'Velocity Speed', 'float', speed)
    if gravity is not None:
        put(system, emitter, 'ParticleUpdateScript', 'GravityForce', 'Gravity', 'vector', gravity)
    if drag is not None:
        put(system, emitter, 'ParticleUpdateScript', 'Drag', 'Drag', 'float', drag)


def far_visible(system, emitters):
    for emitter in emitters:
        result = edit.set_renderer(system, emitter, 0, json.dumps({'bEnableCameraDistanceCulling': False, 'bCastShadows': False}))
        assert not result, (system.get_name(), emitter, result)


def finish(system, note):
    lib.set_metadata_tag(system, 'Source', note)
    assert lib.save_loaded_asset(system, False), system.get_name()
    report[system.get_name()] = system.get_path_name()


# Bright fireball flash: short, very large, HDR orange so it reads on 10 Hz image streams.
flash = derive('NS_War_Flash', 'Coastal/NS_Megablast_Flash')
burst(flash, 'SimpleSpriteBurst', 6, (.5, .8), (1600, 3200), (9., 3.2, .7, 1.), speed=250, radius=250, single_lifetime=True)
far_visible(flash, ['SimpleSpriteBurst'])
finish(flash, 'Rook & Bolt NS_Megablast_Flash, resized for kilometre views')

# Ground burst: thrown grit/debris falling back plus a brown dust cloud.
blast = derive('NS_War_Blast', 'Coastal/NS_Megablast_Blast')
burst(blast, 'OmnidirectionalBurst', 70, (1.2, 2.8), (30, 110), (.22, .18, .13, .95), speed=2600, radius=150, gravity=(0, 0, -980), drag=.4)
burst(blast, 'SimpleSpriteBurst', 30, (2.5, 4.5), (500, 1200), (.40, .33, .24, .75), speed=700, radius=350, single_lifetime=True)
far_visible(blast, ['OmnidirectionalBurst', 'SimpleSpriteBurst'])
# Keep Rook & Bolt's soft dust disc material (RuneSim's M_SmokeSoft renders these emitters as opaque quads).
assert not edit.set_renderer(blast, 'SimpleSpriteBurst', 0, json.dumps({'Material': '/Game/LivingWorld/Effects/RookAndBolt/Coastal/M_Particle_Dust.M_Particle_Dust'}))
finish(blast, 'Rook & Bolt NS_Megablast_Blast, resized for artillery-scale ground bursts')

# Dark rising smoke: one burst per 1.2 s from a fire overlaps into a column.
column = derive('NS_War_SmokeColumn', 'Coastal/NS_Megablast_Smoke')
burst(column, 'OmnidirectionalBurst', 7, (9., 15.), (900, 2000), (.085, .08, .075, .7), speed=140, radius=220, gravity=(40, 20, 320), drag=.6)
far_visible(column, ['OmnidirectionalBurst'])
finish(column, 'Rook & Bolt NS_Megablast_Smoke, resized into a rising battlefield smoke column')

# White obscurant screen: slow, wide, long-lived.
screen = derive('NS_War_SmokeScreen', 'Coastal/NS_SmokeScreen_Coastal')
burst(screen, 'OmnidirectionalBurst', 10, (25., 38.), (2200, 3800), (.78, .79, .81, .55), speed=220, radius=900, gravity=(25, 10, 18), drag=.8)
far_visible(screen, ['OmnidirectionalBurst'])
finish(screen, 'Rook & Bolt NS_SmokeScreen_Coastal, resized into a vehicle smoke screen')

# Flames: flipbook fire sprites re-emitted every 0.5 s by the war layer. The flame occupies a narrow
# band of each atlas frame, so the quads are large (11-26 m) for a 7-10 m visible vehicle fire.
flames = derive('NS_War_Flames', 'Coastal/NS_Vehicle_Flames')
put(flames, 'DirectionalBurst', 'EmitterUpdateScript', 'SpawnBurst_Instantaneous', 'Spawn Count', 'int', 12)
put(flames, 'DirectionalBurst', 'ParticleSpawnScript', 'InitializeParticle', 'Lifetime Min', 'float', .8)
put(flames, 'DirectionalBurst', 'ParticleSpawnScript', 'InitializeParticle', 'Lifetime Max', 'float', 1.4)
put(flames, 'DirectionalBurst', 'ParticleSpawnScript', 'InitializeParticle', 'Sprite Size Min', 'vector2', (1100, 1600))
put(flames, 'DirectionalBurst', 'ParticleSpawnScript', 'InitializeParticle', 'Sprite Size Max', 'vector2', (1700, 2600))
put(flames, 'DirectionalBurst', 'ParticleSpawnScript', 'InitializeParticle', 'Color', 'color', (6., 2.6, .7, 1.))
put(flames, 'DirectionalBurst', 'ParticleSpawnScript', 'AddVelocityInCone', 'Velocity Strength', 'float', 350)
far_visible(flames, ['DirectionalBurst'])
finish(flames, 'Rook & Bolt NS_Vehicle_Flames (16-frame flame atlas), resized for burning wrecks')

# Volumetric fire and smoke (NiagaraFluids Grid3D gas): used only for fires seen large, within a budget.
fluid = derive('NS_War_FluidFire', 'VehicleFluids/NS_Vehicle_FireSmoke')
# Thinner simulated smoke: at the original density the smoke fills the grid and its box edges show from
# side-on tripod views; the sprite smoke column carries the plume above the simulated fire.
volume = unreal.load_asset(DEST + 'MI_War_FluidVolume') or lib.duplicate_asset(SOURCE + 'VehicleFluids/MI_Vehicle_FluidVolume', DEST + 'MI_War_FluidVolume')
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(volume, 'DensityGain', .045)
lib.save_loaded_asset(volume, False)
assert not edit.set_renderer(fluid, 'Grid3D_Gas_Master_Emitter', 1, json.dumps({'Material': volume.get_path_name()}))
# The renderer keeps Rook & Bolt's DensityGain/Albedo overrides, so thin the gas at its source instead:
# less smoke density (no visible grid box from side-on views) and lower temperature (no saturated flame block).
for key, value in (('Density', .6), ('Temperature', .58), ('Radius', 24.)):
    put(fluid, 'ParticleSourceEmitter', 'ParticleUpdateScript', 'SetFluidSourceAttributes', key, 'float', value)
put(fluid, 'ParticleSourceEmitter', 'EmitterUpdateScript', 'SpawnRate', 'SpawnRate', 'float', 260.)
# The template's rigid-mesh collision query gathers every movable actor near the fire (all Living World agents,
# ~1,800 physics boxes), which dropped the packaged demo to ~5 fps. Only actors tagged RuneSimFluidObstacle
# (none by default) now act as fluid obstacles; the terrain still bounds the plume through the sprite layer.
changed = edit.set_data_interface_property(fluid, 'NiagaraDataInterfaceRigidMeshCollisionQuery', 'ActorTags', '("RuneSimFluidObstacle")')
assert changed > 0, changed
assert edit.set_data_interface_property(fluid, 'NiagaraDataInterfaceRigidMeshCollisionQuery', 'MaxNumPrimitives', '16') > 0
report['fluid_collision_queries_restricted'] = changed
finish(fluid, 'Rook & Bolt NS_Vehicle_FireSmoke (from Epic NiagaraFluids Grid3D_Gas_Fire), unchanged simulation')

# Small-arms and fragment impacts: kept close-range sized, but visible from zoomed tripods.
for surface in ('Metal', 'Rock', 'Sand', 'Water'):
    impact = derive('NS_War_Impact_' + surface, 'Coastal/NS_Impact_' + surface)
    far_visible(impact, ['DirectionalBurst'] if surface == 'Metal' else [])
    finish(impact, 'Rook & Bolt NS_Impact_' + surface)

scorch = unreal.load_asset(SOURCE + 'Coastal/M_CoastalScorch')
domain = scorch.get_editor_property('material_domain')
if domain == unreal.MaterialDomain.MD_DEFERRED_DECAL:
    decal = unreal.load_asset(DEST + 'M_War_Scorch') or lib.duplicate_asset(SOURCE + 'Coastal/M_CoastalScorch', DEST + 'M_War_Scorch')
    assert lib.save_loaded_asset(decal, False)  # unsaved duplicates are not cooked
    report['M_War_Scorch'] = decal.get_path_name()
report['scorch_domain'] = str(domain)
print('WAR_EFFECTS', json.dumps(report))
