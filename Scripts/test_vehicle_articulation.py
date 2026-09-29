"""Verify moving jeep articulation in the running demo; no persistent scene edits."""
import json
import time
from pathlib import Path
import unreal

def run_vehicle_test():
    world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    cars=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent)
          if a.active and a.profile.get_name()=='DA_Jeep']
    assert cars
    started=time.monotonic()
    observations=[]
    start={a.get_name():a.get_actor_location() for a in cars}
    def tick(dt):
        observations.extend({'actor':a.get_name(),'speed':a.velocity.length(),
                             'steering':[p.steering_degrees for p in a.wheel_poses],
                             'offsets':[p.offset.length() for p in a.wheel_poses],
                             'proxy':isinstance(a.animated_visual.get_anim_instance(),unreal.LivingVehicleAnimation)}
                            for a in cars)
        if time.monotonic()-started<8:return
        unreal.unregister_slate_post_tick_callback(handle)
        checks={
            'three_jeeps':len(cars)==3,
            'all_cars_move':all((a.get_actor_location()-start[a.get_name()]).length()>50 for a in cars),
            'four_wheel_poses':all(len(s['steering'])==4 for s in observations),
            'vehicle_animation_proxy':all(s['proxy'] for s in observations),
            'front_wheels_follow_turns':any(any(abs(x)>.5 for x in s['steering'][:2]) for s in observations),
            'rear_wheels_do_not_steer':all(all(abs(x)<.01 for x in s['steering'][2:]) for s in observations),
            'steering_bounded':all(all(abs(x)<=35.01 for x in s['steering']) for s in observations),
            'suspension_bounded':all(all(x<=20.1 for x in s['offsets']) for s in observations),
        }
        report={'passed':all(checks.values()),'checks':checks,'agents':len(cars),
                'max_front_steering':max(abs(x) for s in observations for x in s['steering'][:2]),
                'samples':len(observations)}
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/vehicle-integration.json').write_text(json.dumps(report,indent=2))
        print('VEHICLE_INTEGRATION',report)
    handle=unreal.register_slate_post_tick_callback(tick)

run_vehicle_test()
