"""Exercise imported humanoid pose progression and blocked/clear route transitions."""
import unreal
import time
import json
from pathlib import Path

def run_human_locomotion_test():
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==1 and 'LivingWorldDemo' in worlds[0].get_name()
    world=worlds[0]
    humans=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent)
            if a.active and a.profile.kind in (unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER)]
    routes=[r for r in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute)]
    saved_routes=[(r,r.get_editor_property('validated')) for r in routes]
    assert humans
    checks=[]
    phase=0
    started=time.monotonic()
    def clip(a):
        return a.animated_visual.get_anim_instance().get_animation_asset().get_name()
    def bone(a):
        names=[str(a.animated_visual.get_bone_name(i)) for i in range(a.animated_visual.get_num_bones())]
        foot=next(n for n in names if n.endswith('LeftFoot'))
        return a.animated_visual.get_socket_transform(foot,unreal.RelativeTransformSpace.RTS_COMPONENT).translation
    initial={a.get_name():bone(a) for a in humans}
    initial_steps={a.get_name():a.footsteps_played for a in humans}
    def finish(error=None):
        for r,v in saved_routes:r.set_editor_property('validated',v)
        unreal.unregister_slate_post_tick_callback(human_test_handler)
        report={'checks':checks,'error':error,'humans':len(humans),'passed':error is None and len(checks)==5 and all(c['passed'] for c in checks)}
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/human-locomotion.json').write_text(json.dumps(report,indent=2))
        print('HUMAN_LOCOMOTION_RESULT',report)
    def record(name,result):checks.append({'name':name,'passed':bool(result)})
    def tick(dt):
        nonlocal phase,started
        try:
            elapsed=time.monotonic()-started
            if phase==0 and elapsed>1.3:
                record('Weighted skeleton pose advances',any((bone(a)-initial[a.get_name()]).length()>1 for a in humans))
                record('Human blend inputs match actual movement',any(a.velocity.length()>20 for a in humans) and all('BS_' in clip(a) and abs(a.locomotion_speed_ratio-min(2.5,a.velocity.length()/(a.profile.speed_meters_per_second*100)))<.05 for a in humans))
                record('Locomotion emits spatial footsteps',all(a.footsteps_played>initial_steps[a.get_name()] for a in humans))
                for r,v in saved_routes:r.set_editor_property('validated',False)
                phase=1;started=time.monotonic()
            elif phase==1 and elapsed>.6:
                record('Blocked humans stop and idle',all(a.velocity.length()<.1 and a.locomotion_speed_ratio<.01 and 'BS_' in clip(a) for a in humans))
                for r,v in saved_routes:r.set_editor_property('validated',v)
                phase=2;started=time.monotonic()
            elif phase==2 and elapsed>1:
                record('Clear routes resume walking',all(a.velocity.length()>1 and a.locomotion_speed_ratio>.1 and 'BS_' in clip(a) for a in humans))
                finish()
        except Exception as e:finish(str(e))
    human_test_handler=unreal.register_slate_post_tick_callback(tick)

run_human_locomotion_test()
