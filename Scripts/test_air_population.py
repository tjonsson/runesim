"""Exercise the mixed air population, real bone motion, lanes and camera feed."""
import unreal
import time
import json
from pathlib import Path

def run_air_acceptance():
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    existing=unreal.EditorLevelLibrary.get_pie_worlds(False)
    test_world=existing[0] if existing else unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert test_world and 'LivingWorldDemo' in test_world.get_name()
    state={'phase':0,'started':time.monotonic(),'checks':[],'samples':[],'snap':{},'animated':set(),'poses':{}}
    output=Path(unreal.Paths.project_saved_dir())/'LivingWorld/air-population.json'
    def check(name,passed):state['checks'].append({'name':name,'passed':bool(passed)})
    def frame(dt):
        try:
            worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds:return
            world=worlds[0]
            system=next((o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer()==world),None)
            if not system:return
            agents=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent) if a.active]
            if state['phase']==0:
                options=unreal.LivingWorldOptions()
                options.enabled=True; options.preset=unreal.LivingPreset.CUSTOM
                options.planes=4; options.helicopters=4; options.drones=10
                options.bird_flocks=4; options.flock_size=10
                options.crowd_density=1; options.traffic_density=1; options.max_actors=120
                system.apply_options(options)
                camera=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimPTZ)[0]
                camera.set_ptz(45,15,85)
                if globals().get('living_review_start_stream',True):
                    camera.stream.start_stream()
                    camera.set_editor_property('rosbridge_url','ws://192.168.18.9:9090/runesim')
                    camera.set_editor_property('enable_ros',True)
                unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 30')
                state['phase']=1; state['started']=time.monotonic()
            elif state['phase']==1 and time.monotonic()-state['started']>10:
                state['snap']={a.get_name():{'pos':a.get_actor_location(),
                    'bone':a.animated_visual.get_socket_rotation(a.animated_visual.get_bone_name(a.animated_visual.get_num_bones()-1))} for a in agents}
                state['phase']=2; state['started']=time.monotonic()
            elif state['phase']==2:
                state['samples'].append(dt)
                # Component-space poses exclude actor banking/translation. Accumulate changes
                # over successive frames so a whole-number rotor cycle cannot alias to rest.
                for agent in agents:
                    mesh=agent.animated_visual
                    pose=[mesh.get_socket_transform(mesh.get_bone_name(i),unreal.RelativeTransformSpace.RTS_COMPONENT).rotation for i in range(1,mesh.get_num_bones())]
                    previous=state['poses'].get(agent.get_name())
                    if previous and len(previous)==len(pose):
                        if any(abs(q.x*p.x+q.y*p.y+q.z*p.z+q.w*p.w)<.99999 for q,p in zip(pose,previous)):
                            state['animated'].add(agent.profile.get_name())
                    state['poses'][agent.get_name()]=pose
                if time.monotonic()-state['started']<20:return
                air=[a for a in agents if a.profile.kind in (unreal.LivingKind.PLANE,unreal.LivingKind.HELICOPTER,unreal.LivingKind.DRONE,unreal.LivingKind.BIRD)]
                kinds={str(k):sum(a.profile.kind==k for a in air) for k in (unreal.LivingKind.PLANE,unreal.LivingKind.HELICOPTER,unreal.LivingKind.DRONE,unreal.LivingKind.BIRD)}
                names=sorted({a.profile.get_name() for a in air})
                check('All four airborne categories present',all(v>0 for v in kinds.values()))
                check('Eight detailed airborne profiles represented',all('DA_'+n in names for n in ('Mavic3','Orqa','F4','UH1','AH1W','DetailedPigeon','Gull','Crow')))
                check('Actor budget respected',len(agents)<=120)
                moved=[a for a in air if a.get_name() in state['snap'] and (a.get_actor_location()-state['snap'][a.get_name()]['pos']).length()>100]
                check('Airborne agents advance',len(moved)>=len(air)*.85)
                check('Airborne positions stay finite',all(all(abs(v)<1000000 for v in (a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z)) for a in air))
                check('Bank angles respect profile limits',all(abs(a.get_actor_rotation().roll)<=a.profile.max_bank_degrees+.5 for a in air))
                flocks={}
                for a in air:
                    if a.profile.kind==unreal.LivingKind.BIRD:flocks.setdefault(a.flock,set()).add(a.profile.get_name())
                check('Each flock keeps one species',all(len(v)==1 for v in flocks.values()))
                check('Component-space skeletal animation advances for all seven articulated detailed profiles',all('DA_'+n in state['animated'] for n in ('Mavic3','Orqa','UH1','AH1W','DetailedPigeon','Gull','Crow')))
                samples=state['samples']
                report={'checks':state['checks'],'passed':all(c['passed'] for c in state['checks']),
                    'active_count':len(agents),'air_count':len(air),'moving_air_count':len(moved),'categories':kinds,
                    'profiles':names,'animated_profiles':sorted(state['animated']),'mean_frame_ms':1000*sum(samples)/len(samples),'duration_seconds':20}
                output.write_text(json.dumps(report,indent=2))
                unreal.unregister_slate_post_tick_callback(state['handle'])
                print('AIR_ACCEPTANCE',report)
        except Exception as error:
            output.write_text(json.dumps({'passed':False,'error':str(error)},indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle']);raise
    state['handle']=unreal.register_slate_post_tick_callback(frame)
    if not unreal.EditorLevelLibrary.get_pie_worlds(False):levels.editor_play_simulate()

run_air_acceptance()
