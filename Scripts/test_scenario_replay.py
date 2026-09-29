"""Record live animated agents and seek inert copies without moving the subjects."""
import unreal
import time
import json
from pathlib import Path

def run_replay_test():
    world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    agents=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent) if a.active]
    subjects=[next(a for a in agents if a.profile.get_name()=='DA_'+name) for name in ('Jeep','UH1','Gull','Civilian','Crow')]
    system=next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer()==world)
    system.toggle_recording()
    recorder=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimScenarioRecorder)[0]
    recorder.subjects=subjects;recorder.start_recording()
    state={'start':time.monotonic(),'recorder':recorder}
    def finish(dt):
        if time.monotonic()-state['start']<3:return
        unreal.unregister_slate_post_tick_callback(state['handle'])
        checks={};replay=None
        try:
            system.toggle_recording()
            checks['recording_saved']=bool(system.last_recording)
            checks['recording_loaded']=system.play_last_recording()
            replay=system.replay
            before=[a.get_actor_transform() for a in subjects]
            checks['five_visual_tracks']=len(replay.visuals)==5
            frames=[json.loads(line) for line in (Path(unreal.Paths.project_saved_dir())/'LivingWorld/Recordings'/f'{system.last_recording}.jsonl').read_text().splitlines()]
            checks['human_blend_inputs_recorded']=any('BS_Civilian' in actor.get('animation','') and 'blend_position' in actor for frame in frames for actor in frame['actors'])
            jeep_frames=[actor for frame in frames for actor in frame['actors'] if 'Jeep' in actor.get('skeletal_mesh','')]
            checks['vehicle_wheel_poses_recorded']=bool(jeep_frames) and all(len(actor.get('wheel_poses',[]))==4 for actor in jeep_frames)
            checks['seek_midpoint']=replay.seek(replay.duration*.5)
            checks['live_subjects_unchanged']=all((a.get_actor_location()-t.translation).length()<.001 for a,t in zip(subjects,before))
            checks['copies_have_no_collision']=all(v.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for v in replay.visuals)
            checks['skeletal_appearance_preserved']=all(isinstance(v,unreal.SkeletalMeshComponent) and v.get_skeletal_mesh_asset() for v in replay.visuals)
            checks['skeletal_poses_available']=all(v.get_anim_instance() for v in replay.visuals)
            vehicle=replay.visuals[0].get_anim_instance()
            checks['replay_uses_vehicle_wheel_animation']=isinstance(vehicle,unreal.LivingVehicleAnimation) and len(vehicle.wheel_poses)==4
            checks['seek_is_bounded']=replay.seek(9999) and abs(replay.playback_time-replay.duration)<.01
            report={'passed':all(checks.values()),'checks':checks,'duration':replay.duration,'subjects':[a.profile.get_name() for a in subjects]}
        except Exception as error:report={'passed':False,'checks':checks,'error':str(error)}
        finally:
            system.clear_replay()
            if unreal.SystemLibrary.is_valid(recorder):recorder.destroy_actor()
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/replay-integration.json').write_text(json.dumps(report,indent=2));print('REPLAY_INTEGRATION',report)
    state['handle']=unreal.register_slate_post_tick_callback(finish)
run_replay_test()
