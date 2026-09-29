import unreal,json,time
from pathlib import Path
world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
cameras=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimPTZ)
agents=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent) if a.active]
report={'cameras':[{'id':c.camera_id,'connected':c.stream.is_connected(),'frame_number':c.stream.frame_number} for c in cameras],
        'machinery':[]}
for a in agents:
    if a.profile.loop_sound:
        sounds=a.get_components_by_class(unreal.AudioComponent)
        report['machinery'].append({'profile':a.profile.get_name(),'playing':any(s.is_playing() for s in sounds)})
report['passed']=len(cameras)==2 and all(c['connected'] and c['frame_number']>100 for c in report['cameras']) and any(a['playing'] for a in report['machinery'])
(Path(unreal.Paths.project_saved_dir())/'LivingWorld/multicamera-audio.json').write_text(json.dumps(report,indent=2))
print('MULTICAMERA_AUDIO',report)
