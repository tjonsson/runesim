"""Read-only persistence check; run in a fresh editor/commandlet after installation."""
import unreal
import json
from pathlib import Path
checks=[]
for kind in ('Civilian','Soldier'):
    for name in ('Walk','Run'):
        animation=unreal.load_asset('/Game/LivingWorld/Models/'+kind+'/'+kind+'_'+name)
        events=unreal.AnimationLibrary.get_animation_notify_events_for_track(animation,'LivingWorldFootsteps')
        valid=len(events)==2 and all(isinstance(e.get_editor_property('notify'),unreal.LivingFootstepNotify) for e in events)
        checks.append({'animation':animation.get_path_name(),'notifies':len(events),'passed':valid})
report={'checks':checks,'passed':all(c['passed'] for c in checks)}
(Path(unreal.Paths.project_saved_dir())/'LivingWorld/footstep-persistence.json').write_text(json.dumps(report,indent=2))
assert report['passed'],report
print('FOOTSTEP_PERSISTENCE_RESULT',report)
