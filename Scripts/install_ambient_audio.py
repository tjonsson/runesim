"""Import spatial machinery loops, shared mixer class and bounded concurrency."""
import unreal
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
root=Path(unreal.Paths.project_dir());tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.EditorAssetLibrary
folder='/Game/LivingWorld/Audio/Ambient'
sound_class=unreal.load_asset(folder+'/SC_LivingAmbience') or tools.create_asset('SC_LivingAmbience',folder,unreal.SoundClass,unreal.SoundClassFactory())
concurrency=unreal.load_asset(folder+'/Concurrency_Machinery') or tools.create_asset('Concurrency_Machinery',folder,unreal.SoundConcurrency,unreal.SoundConcurrencyFactory())
settings=unreal.SoundConcurrencySettings();settings.max_count=12
settings.resolution_rule=unreal.MaxConcurrentResolutionRule.STOP_QUIETEST
settings.voice_steal_release_time=.25;settings.set_editor_property('volume_scale',.95)
concurrency.set_editor_property('concurrency',settings);lib.save_loaded_asset(concurrency);lib.save_loaded_asset(sound_class)
for name in ('helicopter_loop','drone_loop','utility_engine_loop','jet_loop','woodland_birds'):
    task=unreal.AssetImportTask();task.filename=str(root/'Art/LivingWorld/Prepared/Audio'/(name+'.wav'))
    task.destination_path=folder;task.destination_name=name;task.automated=task.save=task.replace_existing=True
    tools.import_asset_tasks([task]);sound=unreal.load_asset(folder+'/'+name)
    sound.set_editor_property('looping',True);sound.set_editor_property('sound_class_object',sound_class)
    sound.set_editor_property('concurrency_set',{concurrency});lib.save_loaded_asset(sound)
for name,sound in [('UH1','helicopter_loop'),('AH1W','helicopter_loop'),('Mavic3','drone_loop'),('Orqa','drone_loop'),('F4','jet_loop'),('Jeep','utility_engine_loop')]:
    profile=unreal.load_asset('/Game/LivingWorld/Profiles/DA_'+name)
    profile.modify()
    profile.loop_sound=unreal.load_asset(folder+'/'+sound);lib.save_loaded_asset(profile,False)
print('AMBIENT_AUDIO_READY: six machinery profiles, 12 voices maximum, shared class; woodland bed available separately')
