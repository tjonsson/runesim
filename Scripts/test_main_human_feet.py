"""Two-minute foot-support acceptance in actual MainLevel; restores population settings."""
import unreal
import time
import json
import math
from pathlib import Path

def run():
    world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    assert 'MainLevel' in world.get_name()
    system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == world)
    previous = system.get_options()
    options = system.get_options()
    options.enabled = True
    options.crowd_density = 3
    options.population = unreal.LivingPopulation.MIXED
    options.max_actors = 120
    system.apply_options(options)
    started = time.monotonic()
    state = {'samples': [], 'last': 0, 'profiles': set(), 'corrected_profiles': set(), 'stance_profiles': set(), 'bad': [], 'max_cm': 0, 'max_stance_cm': 0}
    ptz = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimPTZ)[0]
    initial_frames = ptz.stream.frame_number
    def finish(error=None):
        unreal.unregister_slate_post_tick_callback(handle)
        system.apply_options(previous)
        recent = state['samples'][-30:]
        checks = {
            'dense_humans': bool(recent) and min(s['humans'] for s in recent) >= 40,
            'both_rigs_use_grounding': len(state['profiles']) == 2,
            'both_rigs_correct_real_terrain': len(state['corrected_profiles']) == 2,
            'foot_offsets_finite_and_bounded': not state['bad'] and state['max_cm'] <= 18.001,
            'both_rigs_use_stance_correction': len(state['stance_profiles']) == 2,
            'stance_offsets_finite_and_bounded': not state['bad'] and state['max_stance_cm'] <= 18.001,
            'movement_preserved': bool(recent) and sum(s['moving']/max(1,s['humans']) for s in recent)/len(recent) >= .8,
            'stream_advances': ptz.stream.is_connected() and ptz.stream.frame_number-initial_frames > 1000,
        }
        report = {'passed': error is None and all(checks.values()), 'checks': checks, 'error': error,
                  'max_correction_cm': state['max_cm'], 'max_stance_cm': state['max_stance_cm'], 'samples': state['samples'], 'invalid': state['bad']}
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-human-stance.json').write_text(json.dumps(report,indent=2))
        print('MAIN_HUMAN_FEET',report['passed'],checks,error)
    def tick(dt):
        elapsed = time.monotonic()-started
        if elapsed < 20 or elapsed-state['last'] < 1: return
        state['last'] = elapsed
        try:
            humans = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent)
                      if a.active and a.profile.kind in (unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER)]
            supported = corrected = planted = 0
            for actor in humans:
                animation = actor.animated_visual.get_anim_instance()
                if not isinstance(animation,unreal.LivingHumanAnimation):
                    state['bad'].append(actor.get_name()+': wrong animation instance'); continue
                state['profiles'].add(actor.profile.get_name())
                supported += animation.supported_feet
                for foot in animation.foot_supports:
                    h = foot.height_cm
                    if not math.isfinite(h) or abs(h)>18.001: state['bad'].append(actor.get_name()+': invalid height')
                    state['max_cm'] = max(state['max_cm'],abs(h))
                    offset = foot.stance_offset_cm.length()
                    if not math.isfinite(offset) or offset>18.001: state['bad'].append(actor.get_name()+': invalid stance')
                    state['max_stance_cm'] = max(state['max_stance_cm'],offset)
                    if offset>1:
                        planted += 1
                        state['stance_profiles'].add(actor.profile.get_name())
                    if abs(h)>.5:
                        corrected += 1
                        state['corrected_profiles'].add(actor.profile.get_name())
            state['samples'].append({'seconds':round(elapsed,2),'humans':len(humans),
                                     'moving':sum(a.velocity.length()>30 for a in humans),
                                     'supported_feet':supported,'corrected_feet':corrected,'stance_feet':planted})
            if elapsed>=120:finish()
        except Exception as error:finish(repr(error))
    handle = unreal.register_slate_post_tick_callback(tick)
    print('MAIN_HUMAN_FEET_STARTED')

run()
