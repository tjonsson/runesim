"""Install combat audio, bird calls/perch clips, surface footsteps; enable MainLevel engagement.

Run through Scripts/ue_remote.py in the editor with MainLevel open and PIE stopped.
Inputs are produced by synthesize_living_audio.py and add_bird_perch_clips.py.
"""
import json
import unreal
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
project = Path(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
audio = project / 'Art/LivingWorld/Prepared/Audio'
report = {}


def save(asset):
    asset.modify()
    assert lib.save_loaded_asset(asset, False), asset.get_path_name()


def import_file(source, folder, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(source); task.destination_path = folder; task.destination_name = name
    task.automated = task.save = task.replace_existing = True
    if options:
        task.options = options
    tools.import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths'))


def asset(folder, name, cls, factory):
    existing = unreal.load_asset(folder + '/' + name)
    return existing or tools.create_asset(name, folder, cls, factory)


def attenuation(folder, name, falloff, radius):
    att = asset(folder, name, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = att.get_editor_property('attenuation')
    settings.set_editor_property('attenuate', True)
    settings.set_editor_property('spatialize', True)
    settings.set_editor_property('falloff_distance', falloff)
    settings.set_editor_property('attenuation_shape_extents', unreal.Vector(radius, 0, 0))
    att.set_editor_property('attenuation', settings)
    save(att)
    return att


def concurrency(folder, name, voices):
    con = asset(folder, name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    settings = unreal.SoundConcurrencySettings(); settings.max_count = voices
    settings.resolution_rule = unreal.MaxConcurrentResolutionRule.STOP_QUIETEST
    con.set_editor_property('concurrency', settings)
    save(con)
    return con


def sound(source, folder, name, att=None, con=None, looping=False, sound_class=None):
    import_file(source, folder, name)
    wave = unreal.load_asset(folder + '/' + name)
    assert isinstance(wave, unreal.SoundWave), name
    wave.set_editor_property('looping', looping)
    if att: wave.set_editor_property('attenuation_settings', att)
    if con: wave.set_editor_property('concurrency_set', {con})
    if sound_class: wave.set_editor_property('sound_class_object', sound_class)
    save(wave)
    return wave


ambient_class = unreal.load_asset('/Game/LivingWorld/Audio/Ambient/SC_LivingAmbience')

# Combat: launch, motor loop, explosion. Heard from far away; bounded voices.
combat = '/Game/LivingWorld/Audio/Combat'
combat_att = attenuation(combat, 'SA_Combat', 60000., 1500.)
combat_con = concurrency(combat, 'Concurrency_Combat', 10)
names = {'missile_launch': 'S_MissileLaunch', 'missile_motor': 'S_MissileMotor', 'explosion': 'S_Explosion'}
for source, name in names.items():
    sound(audio / 'Combat' / (source + '.wav'), combat, name, combat_att, combat_con, looping=name == 'S_MissileMotor', sound_class=ambient_class)
report['combat_sounds'] = sorted(names.values())

# Bird calls, per species.
birds = '/Game/LivingWorld/Audio/Birds'
bird_att = attenuation(birds, 'SA_BirdCalls', 7000., 300.)
bird_con = concurrency(birds, 'Concurrency_BirdCalls', 6)
calls = {}
for species, profile, interval in (('pigeon_coo', 'DetailedPigeon', 18.), ('gull_call', 'Gull', 14.), ('crow_caw', 'Crow', 12.)):
    calls[profile] = ([sound(source, birds, source.stem, bird_att, bird_con, sound_class=ambient_class)
                       for source in sorted((audio / 'Birds').glob(species + '_*.wav'))], interval)

# Perch clips on the existing skeletons; flight clips and meshes are untouched.
for name in ('DetailedPigeon', 'Gull', 'Crow'):
    folder = '/Game/LivingWorld/Models/Flight/' + name
    mesh = unreal.load_asset(folder + '/SK_' + name)
    assert mesh, name
    info = json.loads((project / 'Art/LivingWorld/Prepared/Flight' / (name + '_Perch.json')).read_text())
    options = unreal.FbxImportUI()
    options.import_mesh = False; options.import_animations = True; options.import_as_skeletal = True
    options.import_materials = False; options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.automated_import_should_detect_type = False
    options.skeleton = mesh.skeleton
    import_file(info['fbx'], folder + '/Perch', name + '_Perch', options)
    clips = [a.get_asset() for a in registry.get_assets_by_path(folder + '/Perch', recursive=False)]
    clips = [c for c in clips if isinstance(c, unreal.AnimSequence)]
    pick = {clip: next(c for c in clips if c.get_name().endswith(clip)) for clip in ('Landing', 'Perched', 'TakeOff')}
    for clip in pick.values():
        assert clip.get_play_length() > .3, clip.get_name()
    profile = unreal.load_asset('/Game/LivingWorld/Profiles/DA_' + name)
    profile.set_editor_property('landing_animation', pick['Landing'])
    profile.set_editor_property('idle_animation', pick['Perched'])
    profile.set_editor_property('takeoff_animation', pick['TakeOff'])
    profile.set_editor_property('allow_perching', True)
    profile.set_editor_property('perch_height_cm', info['perch_height_cm'])
    profile.set_editor_property('call_sounds', calls[name][0])
    profile.set_editor_property('call_interval_seconds', calls[name][1])
    save(profile)
    report[name] = {'clips': {k: (v.get_path_name(), round(v.get_play_length(), 2)) for k, v in pick.items()},
                    'perch_height_cm': info['perch_height_cm'], 'calls': len(calls[name][0])}

# Surface footsteps for both human profiles (reviewed MainLevel corridors are a dirt road and shoulder).
steps = '/Game/LivingWorld/Audio/Footsteps'
surfaces = {}
for surface in ('dirt', 'gravel', 'grass'):
    surfaces[surface] = [sound(source, steps, source.stem) for source in sorted((audio / 'Footsteps').glob(f'footstep_{surface}_*.wav'))]
    assert len(surfaces[surface]) == 5, surface
for entry in registry.get_assets_by_path('/Game/LivingWorld', recursive=True):
    profile = entry.get_asset()
    if isinstance(profile, unreal.LivingAssetProfile) and profile.kind in (unreal.LivingKind.CIVILIAN, unreal.LivingKind.SOLDIER):
        profile.set_editor_property('dirt_footstep_sounds', surfaces['dirt'])
        profile.set_editor_property('gravel_footstep_sounds', surfaces['gravel'])
        profile.set_editor_property('grass_footstep_sounds', surfaces['grass'])
        save(profile)

# MainLevel: opt the existing tripod into virtual engagement; tag reviewed corridors as dirt.
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'MainLevel', world.get_name()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
ptz = [a for a in actors if isinstance(a, unreal.SimPTZ)]
assert len(ptz) == 1, ptz
ptz[0].modify()
ptz[0].set_editor_property('allow_simulated_engagement', True)
routes = [a for a in actors if isinstance(a, unreal.LivingRoute) and a.validated]
for route in routes:
    route.modify()
    route.set_editor_property('surface', unreal.LivingSurface.DIRT)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
report['mainlevel'] = {'ptz': ptz[0].get_actor_label(), 'camera_id': ptz[0].camera_id, 'ros': ptz[0].enable_ros,
                       'dirt_routes': [r.get_actor_label() for r in routes]}
(project / 'Saved/LivingWorld/combat-content.json').write_text(json.dumps(report, indent=2))
print('COMBAT_CONTENT_READY', json.dumps(report))
