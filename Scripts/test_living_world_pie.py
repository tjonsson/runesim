"""Run in the dedicated demo map; writes an asynchronous PIE integration report."""
import unreal
import time
import json
from pathlib import Path

def run_population_test():
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    pie_worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    test_world=pie_worlds[0] if pie_worlds else editor.get_editor_world()
    if not test_world or 'LivingWorldDemo' not in test_world.get_name():
        raise RuntimeError('Integration test requires LivingWorldDemo')
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    report={'checks':[],'errors':[],'frame_times':[]}
    phase=0; started=time.monotonic(); snapshot={}; original=None
    def check(name,result):
        report['checks'].append({'name':name,'passed':bool(result)})
        if not result: report['errors'].append(name)
    def frame(dt):
        nonlocal phase, started, snapshot, original, handler
        try:
            worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds: return
            world=worlds[0]
            subsystem=next((o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer()==world),None)
            if not subsystem: return
            report['frame_times'].append(dt)
            agents=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent))
            active=[a for a in agents if a.get_editor_property('active')]
            if phase==0:
                original=subsystem.get_options()
                if 'living_demo_previous_options' not in globals():
                    globals()['living_demo_previous_options']=original
                options=unreal.LivingWorldOptions()
                options.enabled=True; options.planes=0; options.helicopters=0; options.drones=0
                options.crowd_density=1; options.traffic_density=1; options.bird_flocks=1; options.flock_size=6
                options.max_actors=30
                subsystem.apply_options(options)
                phase=1; started=time.monotonic()
            elif phase==1 and time.monotonic()-started>6:
                check('Population spawns',len(active)>=10)
                check('Population obeys cap',len(active)<=30)
                check('Ground agents above collision surface',all(a.get_actor_location().z>=20 for a in active if a.profile.kind in [unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER,unreal.LivingKind.CAR]))
                check('Ground actors stay upright',all(abs(a.get_actor_rotation().pitch)<5 and abs(a.get_actor_rotation().roll)<5 for a in active if a.profile.kind in [unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER,unreal.LivingKind.CAR]))
                check('Ground traces ignore other agents',all(a.get_actor_location().z<150 for a in active if a.profile.kind in [unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER,unreal.LivingKind.CAR]))
                snapshot={a.get_name():a.get_actor_location() for a in active}
                phase=2; started=time.monotonic()
            elif phase==2 and time.monotonic()-started>3:
                moved=sum(1 for a in active if a.get_name() in snapshot and (a.get_actor_location()-snapshot[a.get_name()]).length()>50)
                check('Agents move along routes and flight paths',moved>=5)
                report['active_count']=len(active); report['moving_count']=moved; report['pool_before_disable']=len(agents)
                report['states']=[str(a.get_editor_property('behavior')) for a in active]
                options=subsystem.get_options(); options.enabled=False; subsystem.apply_options(options)
                phase=3; started=time.monotonic()
            elif phase==3 and time.monotonic()-started>1:
                check('Off removes all ambient activity',not active)
                check('Player pawn preserved',unreal.GameplayStatics.get_player_pawn(world,0) is not None)
                options=subsystem.get_options(); options.enabled=True; subsystem.apply_options(options)
                phase=4; started=time.monotonic()
            elif phase==4 and time.monotonic()-started>4:
                check('Re-enable reuses bounded pool',len(agents)<=max(30,report['pool_before_disable']) and 0<len(active)<=30)
                subsystem.toggle_menu()
                report['status']=subsystem.get_editor_property('status')
                report['mean_frame_ms']=1000*sum(report['frame_times'])/max(1,len(report['frame_times']))
                report.pop('frame_times')
                (Path(unreal.Paths.project_saved_dir())/'LivingWorld/pie-report.json').write_text(json.dumps(report,indent=2))
                unreal.unregister_slate_post_tick_callback(handler)
                print('LIVING_PIE_RESULT',report)
                # Leave the demo running with the menu open for visual review. Restore preferences later.
        except Exception as error:
            report['errors'].append(str(error))
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/pie-report.json').write_text(json.dumps(report,indent=2))
            unreal.unregister_slate_post_tick_callback(handler)
            raise
    handler=unreal.register_slate_post_tick_callback(frame)
    if not unreal.EditorLevelLibrary.get_pie_worlds(False): levels.editor_play_simulate()

run_population_test()
