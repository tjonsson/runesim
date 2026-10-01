"""Author the missile trail and air-burst Niagara systems (editor, PIE stopped).

NS_MissileTrail: a few small, light-grey, short-lived puffs per spawn; the interceptor spawns one
every 0.1 s, so the trail stays thin and does not block the chase camera.
NS_AirBurst: a large fireball flash that lasts long enough to be seen on 10 Hz image streams,
debris and a dark smoke cloud; used for interceptor detonations and aircraft shoot-downs.
Uses the same experimental authoring adapter as build_living_effects.py.
"""
import json
import unreal
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
lib = unreal.EditorAssetLibrary
folder = '/Game/LivingWorld/Effects'
edit = unreal.LivingEffectAuthoring
material = unreal.load_asset(folder + '/M_Smoke')
assert material, 'Run build_living_effects.py first (M_Smoke)'


def put(system, emitter, stack, module, key, kind, value):
    values = list(value) if isinstance(value, (tuple, list)) else [value]
    result = edit.set_input(system, emitter, stack, module, key, kind, unreal.Vector4(*(values + [0] * (4 - len(values)))))
    assert not result, (system.get_name(), emitter, module, key, result)


def init(system, emitter, **values):
    for key, value in values.items():
        put(system, emitter, 'ParticleSpawnScript', 'InitializeParticle', key.replace('_', ' '),
            'color' if isinstance(value, tuple) else 'float', value)


def spawn(system, emitter, count):
    put(system, emitter, 'EmitterUpdateScript', 'SpawnBurst_Instantaneous', 'Spawn Count', 'int', count)


def clone(name):
    return unreal.load_asset(folder + '/' + name) or lib.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion', folder + '/' + name)


smoke, flash, debris = 'OmnidirectionalBurst', 'SimpleSpriteBurst', 'UpwardMeshBurst'
report = []

trail = clone('NS_MissileTrail')
spawn(trail, smoke, 3)
init(trail, smoke, Lifetime_Min=2., Lifetime_Max=3., Uniform_Sprite_Size_Min=45., Uniform_Sprite_Size_Max=90., Color=(.62, .63, .66, .32))
put(trail, smoke, 'ParticleSpawnScript', 'ShapeLocation', 'Sphere Radius', 'float', 12.)
put(trail, smoke, 'ParticleSpawnScript', 'AddVelocity', 'Velocity Speed', 'float', 25.)
put(trail, smoke, 'ParticleUpdateScript', 'GravityForce', 'Gravity', 'vector', (0, 0, 20))
put(trail, smoke, 'ParticleUpdateScript', 'Drag', 'Drag', 'float', 1.)
assert not edit.set_renderer(trail, smoke, 0, json.dumps({'Material': material.get_path_name()}))
spawn(trail, flash, 0); spawn(trail, debris, 0)
lib.set_metadata_tag(trail, 'Source', 'Epic bundled SimpleExplosion; project-authored thin missile trail')
lib.save_loaded_asset(trail, False); report.append(trail.get_path_name())

burst = clone('NS_AirBurst')
spawn(burst, smoke, 30)
init(burst, smoke, Lifetime_Min=5., Lifetime_Max=8., Uniform_Sprite_Size_Min=250., Uniform_Sprite_Size_Max=450., Color=(.13, .12, .11, .6))
put(burst, smoke, 'ParticleSpawnScript', 'ShapeLocation', 'Sphere Radius', 'float', 150.)
put(burst, smoke, 'ParticleSpawnScript', 'AddVelocity', 'Velocity Speed', 'float', 220.)
put(burst, smoke, 'ParticleUpdateScript', 'GravityForce', 'Gravity', 'vector', (0, 0, 40))
put(burst, smoke, 'ParticleUpdateScript', 'Drag', 'Drag', 'float', 1.2)
assert not edit.set_renderer(burst, smoke, 0, json.dumps({'Material': material.get_path_name()}))
spawn(burst, flash, 8)
init(burst, flash, Lifetime=.9, Uniform_Sprite_Size_Min=450., Uniform_Sprite_Size_Max=800., Color=(14., 3.2, .45, 1.))
spawn(burst, debris, 24)
init(burst, debris, Lifetime_Min=1., Lifetime_Max=2.2, Mesh_Uniform_Scale_Min=.04, Mesh_Uniform_Scale_Max=.12, Color=(.07, .06, .05, 1.))
lib.set_metadata_tag(burst, 'Source', 'Epic bundled SimpleExplosion; project-authored air burst (long flash, debris, smoke)')
lib.save_loaded_asset(burst, False); report.append(burst.get_path_name())

(Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/combat-effects.json').write_text(json.dumps(report, indent=2))
print('COMBAT_EFFECTS_AUTHORED', report)
