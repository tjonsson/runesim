"""Record both live human rigs, then inspect their inert terrain-aware replay."""
import unreal,time,json,re
from pathlib import Path

def run():
    world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent)
    subjects=[next(a for a in actors if a.active and a.profile.get_name()=='DA_'+kind) for kind in ('Civilian','Soldier')]
    system=next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer()==world)
    assert not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimScenarioRecorder), 'An existing recording is in progress'
    assert not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimReplay), 'An existing replay is in progress'
    system.toggle_recording()
    recorder=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimScenarioRecorder)[0]
    recorder.subjects=subjects;recorder.start_recording()
    started=time.monotonic()
    def finish(dt):
        if time.monotonic()-started<4:return
        unreal.unregister_slate_post_tick_callback(handle)
        replay=None;checks={}
        try:
            system.toggle_recording()
            name=system.last_recording
            checks['saved']=bool(name)
            checks['loaded']=system.play_last_recording()
            replay=system.replay
            replay.pause_replay()
            before=[a.get_actor_transform() for a in subjects]
            checks['seek']=replay.seek(replay.duration*.5)
            checks['two_rigs']=len(replay.visuals)==2
            checks['recorded_supports_restored']=all(isinstance(v.get_anim_instance(),unreal.LivingHumanAnimation)
                and len(v.get_anim_instance().foot_supports)==2 for v in replay.visuals)
            checks['live_subjects_unchanged']=all(a.get_actor_transform().translation==t.translation for a,t in zip(subjects,before))
            checks['inert_copies']=all(v.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for v in replay.visuals)
            frames=[json.loads(line) for line in (Path(unreal.Paths.project_saved_dir())/'LivingWorld/Recordings'/f'{name}.jsonl').read_text().splitlines()]
            checks['contacts_in_every_pose']=bool(frames) and all(len(a.get('foot_supports',[]))==2 for f in frames for a in f['actors'])
            checks['stance_in_every_contact']=bool(frames) and all('stance_offset_cm' in s for f in frames for a in f['actors'] for s in a.get('foot_supports',[]))
            def vector(text): return [float(v) for v in re.findall(r'=([-+\d.eE]+)',text)]
            checks['nonzero_stance_recorded']=any(sum(v*v for v in vector(s.get('stance_offset_cm','')))>1 for f in frames for a in f['actors'] for s in a.get('foot_supports',[]))
            target=frames[0]['simulation_time']+replay.playback_time
            left=max(i for i,f in enumerate(frames) if f['simulation_time']<=target)
            right=min(left+1,len(frames)-1)
            interval=frames[right]['simulation_time']-frames[left]['simulation_time']
            alpha=(target-frames[left]['simulation_time'])/interval if interval>0 else 0
            exact=True
            for i,visual in enumerate(replay.visuals):
                for j,foot in enumerate(visual.get_anim_instance().foot_supports):
                    a=vector(frames[left]['actors'][i]['foot_supports'][j]['stance_offset_cm'])
                    b=vector(frames[right]['actors'][i]['foot_supports'][j]['stance_offset_cm'])
                    expected=unreal.Vector(*[x+(y-x)*alpha for x,y in zip(a,b)])
                    exact &= (foot.stance_offset_cm-expected).length()<.01
            checks['seek_interpolates_recorded_stance']=exact
            report={'passed':all(checks.values()),'checks':checks,'recording':name,'frames':len(frames)}
        except Exception as error:report={'passed':False,'checks':checks,'error':repr(error)}
        finally:
            system.clear_replay()
            if unreal.SystemLibrary.is_valid(recorder):recorder.destroy_actor()
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/human-stance-replay.json').write_text(json.dumps(report,indent=2))
        print('HUMAN_FOOT_REPLAY',report)
    handle=unreal.register_slate_post_tick_callback(finish)
run()
