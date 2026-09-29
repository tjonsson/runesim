"""Exercise real pigeon clips, reservation, terrain rejection and takeoff in demo PIE.

Temporarily moves PIE copies; leaves population preferences and authored sites intact.
"""
import unreal
import time
import json
from pathlib import Path

def run_bird_test():
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==1 and 'LivingWorldDemo' in worlds[0].get_name()
    world=worlds[0]
    birds=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent)
           if a.active and a.profile.kind==unreal.LivingKind.BIRD and a.profile.allow_perching]
    assert len(birds)>=2
    # Existing subsystem-owned birds continue to receive ordinary simulation ticks.
    bird,other=birds[:2]
    original=[(a,a.get_actor_transform()) for a in (bird,other)]
    bird.take_off();other.take_off()
    bird.set_actor_location(unreal.Vector(0,0,600),False,False)
    other.set_actor_location(unreal.Vector(0,700,600),False,False)
    site,missing=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingPerch))[:2]
    original_sites=[(p,p.get_actor_transform(),p.get_editor_property('validated')) for p in (site,missing)]
    for p,_,_ in original_sites:
        if p.occupant:p.occupant.take_off()
        p.set_editor_property('validated',False)
    site.set_actor_location(unreal.Vector(0,0,60),False,True)
    missing.set_actor_location(unreal.Vector(0,0,1000),False,True)
    checks=[];seen=set();phase=-1;started=time.monotonic();takeoff_z=0
    def check(name,value):
        checks.append({'name':name,'passed':bool(value)})
    def clip():
        asset=bird.animated_visual.get_anim_instance().get_animation_asset()
        return asset.get_name() if asset else ''
    def finish(error=None):
        unreal.unregister_slate_post_tick_callback(handle)
        bird.take_off();other.take_off()
        for a,transform in original:a.set_actor_transform(transform,False,True)
        for p,transform,validated in original_sites:
            p.set_actor_transform(transform,False,True)
            p.set_editor_property('validated',validated)
        report={'checks':checks,'states_seen':sorted(seen),'error':error,
                'passed':error is None and len(checks)==10 and all(c['passed'] for c in checks)}
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/bird-behavior.json').write_text(json.dumps(report,indent=2))
        print('BIRD_BEHAVIOR_RESULT',report)
    def tick(dt):
        nonlocal phase,started,takeoff_z
        try:
            elapsed=time.monotonic()-started
            state=str(bird.flight_state);seen.add(state)
            if phase==-1 and elapsed>3:
                bird.set_actor_location(unreal.Vector(0,0,600),False,True)
                other.set_actor_location(unreal.Vector(0,700,600),False,True)
                check('Unvalidated perch rejected',not bird.request_perch(site))
                missing.set_editor_property('validated',True)
                check('Missing terrain rejected',not bird.request_perch(missing))
                site.set_editor_property('validated',True)
                check('Reviewed clear perch reserved',bird.request_perch(site))
                check('Reservation excludes second bird',not other.request_perch(site))
                phase=0;started=time.monotonic()
            elif phase==0 and bird.flight_state==unreal.LivingFlightState.PERCHED:
                check('Approach lands without floor penetration',20<bird.get_actor_location().z<30)
                check('Standing uses idle clip','Standing_Idle' in clip() and bird.velocity.length()<.1)
                takeoff_z=bird.get_actor_location().z
                site.set_editor_property('validated',False)
                phase=1;started=time.monotonic()
            elif phase==1 and elapsed>.4:
                check('Withdrawn validation triggers takeoff',bird.flight_state==unreal.LivingFlightState.TAKING_OFF and 'Takeoff' in clip())
                check('Takeoff rises away from the surface',bird.get_actor_location().z>takeoff_z+20)
                site.set_editor_property('validated',True)
                check('Departed bird releases reservation',other.request_perch(site))
                phase=2;started=time.monotonic()
            elif phase==2 and bird.flight_state==unreal.LivingFlightState.GLIDING:
                check('Cruise alternates to gliding clip','Gliding' in clip())
                finish()
            elif elapsed>25:finish('Timed out waiting for phase '+str(phase)+'; '+state)
        except Exception as exc:finish(str(exc))
    handle=unreal.register_slate_post_tick_callback(tick)

run_bird_test()
