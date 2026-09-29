"""Import CC0 footstep variations and add dedicated locomotion notify tracks."""
import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
tools=unreal.AssetToolsHelpers.get_asset_tools()
sounds=[]
for source in sorted((root/'Art/LivingWorld/Prepared/Audio').glob('footstep_concrete_*.wav')):
    task=unreal.AssetImportTask()
    task.filename=str(source)
    task.destination_path='/Game/LivingWorld/Audio/Footsteps'
    task.destination_name=source.stem
    task.automated=task.save=task.replace_existing=True
    tools.import_asset_tasks([task])
    sound=unreal.load_asset(task.destination_path+'/'+source.stem)
    assert isinstance(sound,unreal.SoundWave)
    sounds.append(sound)
assert len(sounds)==5
registry=unreal.AssetRegistryHelpers.get_asset_registry()
for folder in ('/Game/LivingWorld/Profiles','/Game/LivingWorld/Demo'):
    for entry in registry.get_assets_by_path(folder,recursive=True):
        profile=entry.get_asset()
        if isinstance(profile,unreal.LivingAssetProfile) and profile.kind in (unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER):
            profile.set_editor_property('footstep_sounds',sounds)
            unreal.EditorAssetLibrary.save_loaded_asset(profile)
for kind in ('Civilian','Soldier'):
    for name in ('Walk','Run'):
        animation=unreal.load_asset('/Game/LivingWorld/Models/'+kind+'/'+kind+'_'+name)
        # AnimationLibrary mutates notify arrays without marking the package dirty.
        # Record the edit and force a disk save so notifies survive editor restart.
        animation.modify()
        track='LivingWorldFootsteps'
        if unreal.AnimationLibrary.is_valid_anim_notify_track_name(animation,track):
            unreal.AnimationLibrary.remove_animation_notify_events_by_track(animation,track)
        else:
            unreal.AnimationLibrary.add_animation_notify_track(animation,track,unreal.LinearColor(.2,.7,.3,1))
        # Two contacts per complete left/right gait cycle. Dedicated track is
        # editable for later per-surface/foot-IK tuning without touching the rig.
        for fraction in (.03,.53):
            notify=unreal.AnimationLibrary.add_animation_notify_event(animation,track,
                animation.get_play_length()*fraction,unreal.LivingFootstepNotify)
            assert notify
        assert unreal.EditorAssetLibrary.save_loaded_asset(animation,only_if_is_dirty=False)
        print('FOOTSTEPS_INSTALLED',animation.get_name(),len(unreal.AnimationLibrary.get_animation_notify_events_for_track(animation,track)))
