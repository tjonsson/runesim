"""Assign the reviewed bird clips and eight explicit demo-only landing sites."""
import unreal
import math

assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
registry=unreal.AssetRegistryHelpers.get_asset_registry()
clips={}
for data in registry.get_assets_by_path('/Game/LivingWorld/Models/Pigeon',recursive=True):
    asset=data.get_asset()
    if isinstance(asset,unreal.AnimSequence):
        for name in ('Gliding','Standing_Idle','Landing','Takeoff'):
            if name in asset.get_name(): clips[name]=asset
assert len(clips)==4, list(clips)
for data in registry.get_assets_by_path('/Game/LivingWorld',recursive=True):
    profile=data.get_asset() if data.asset_class_path.asset_name=='LivingAssetProfile' else None
    # These clips belong only to the licensed pigeon's skeleton. Detailed
    # generated birds have their own wing rigs and must retain their own clips.
    if profile and profile.kind==unreal.LivingKind.BIRD and profile.skeletal_mesh and profile.skeletal_mesh.skeleton==clips['Gliding'].skeleton:
        for key,value in {'glide_animation':clips['Gliding'],'idle_animation':clips['Standing_Idle'],
                          'landing_animation':clips['Landing'],'takeoff_animation':clips['Takeoff'],
                          'allow_perching':True}.items():profile.set_editor_property(key,value)
        unreal.EditorAssetLibrary.save_loaded_asset(profile)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert 'LivingWorldDemo' in world.get_name(), 'Perch placement is scoped to the demo'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
for index in range(8):
    label=f'LivingDemo_Perch_{index+1}'
    angle=index*math.tau/8
    perch=existing.get(label) or actors.spawn_actor_from_class(unreal.LivingPerch,unreal.Vector(9000*math.cos(angle),9000*math.sin(angle),60))
    perch.set_actor_label(label)
    perch.set_editor_property('validated',True)
    perch.set_editor_property('clearance_cm',25)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('BIRD_CLIPS_AND_DEMO_PERCHES_READY')
