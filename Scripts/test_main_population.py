"""Live acceptance on the actual Cesium MainLevel, including camera-away tile retention."""
import unreal,time,json,collections
from pathlib import Path
def run_main_acceptance():
    previous=globals().pop('main_population_handle',None)
    if previous is not None:
        unreal.unregister_slate_post_tick_callback(previous)
    world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    assert 'MainLevel' in world.get_name()
    system=next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer()==world)
    options=unreal.LivingWorldOptions()
    options.enabled=True;options.preset=unreal.LivingPreset.CUSTOM;options.population=unreal.LivingPopulation.MIXED
    options.planes=4;options.helicopters=4;options.drones=10;options.bird_flocks=4;options.flock_size=10
    options.crowd_density=1;options.traffic_density=1;options.max_actors=120;options.activity_radius_meters=500
    system.apply_options(options)
    unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 30')
    ptz=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimPTZ)[0]
    ptz.set_ptz(30,18,85)
    started_utc=time.time()
    state={'start':time.monotonic(),'last':0,'samples':[],'ground_moving':set(),'air_moving':set(),'profiles':set(),'air_support':[],'frames':ptz.stream.frame_number,'turned':False}
    def tick(dt):
        elapsed=time.monotonic()-state['start']
        if elapsed<20 or elapsed-state['last']<1:return
        state['last']=elapsed
        try:
            agents=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent) if a.active]
            kinds=collections.Counter(str(a.profile.kind) for a in agents)
            blocked=collections.Counter(str(a.profile.kind) for a in agents if a.behavior==unreal.LivingBehavior.BLOCKED)
            moving=collections.Counter(str(a.profile.kind) for a in agents if a.velocity.length()>30)
            state['samples'].append({'elapsed':elapsed,'counts':dict(kinds),'blocked':dict(blocked),'moving':dict(moving),'stream_frames':ptz.stream.frame_number})
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-population-progress.json').write_text(json.dumps(state['samples'][-1],indent=2))
            geo=unreal.CesiumGeoreference.get_default_georeference(world)
            for a in agents:
                kind=a.profile.kind
                state['profiles'].add(a.profile.get_name())
                ground=kind in (unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER,unreal.LivingKind.CAR)
                if a.velocity.length()>30:(state['ground_moving'] if ground else state['air_moving']).add(str(kind))
                if not ground:
                    p=a.get_actor_location()
                    rotation=geo.transform_east_south_up_rotator_to_unreal(unreal.Rotator(),p)
                    up=unreal.MathLibrary.get_up_vector(rotation)
                    hit=unreal.SystemLibrary.line_trace_single_for_objects(world,p+up*300000,p-up*500000,[unreal.ObjectTypeQuery.ECC_WORLD_STATIC],True,[a],unreal.DrawDebugTrace.NONE)
                    if hit:
                        fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
                        clearance=unreal.MathLibrary.dot_vector_vector(p-fields[5],up)
                        state['air_support'].append({'kind':str(kind),'clearance_cm':clearance,'radius_cm':a.profile.collision_radius_cm})
                    else:state['air_support'].append({'kind':str(kind),'missing':True,'blocked':a.behavior==unreal.LivingBehavior.BLOCKED})
            if elapsed>40 and not state['turned']:
                controller=unreal.GameplayStatics.get_player_controller(world,0)
                controller.set_view_target_with_blend(ptz)
                ptz.set_ptz(160,80,85)
                state['away_camera']=ptz.get_name()
                state['turned']=True
            if 50<elapsed<79:
                state['away_observed']=state.get('away_observed',False) or unreal.GameplayStatics.get_player_camera_manager(world,0).get_camera_rotation().pitch>60
            if elapsed<80:return
            unreal.unregister_slate_post_tick_callback(handle)
            ptz.set_ptz(30,18,85)
            checks={
                'actual_mainlevel_with_cesium':bool(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Cesium3DTileset)),
                'all_four_air_categories_moving':len(state['air_moving'])==4,
                'all_three_ground_categories_moving':len(state['ground_moving'])==3,
                'twelve_profiles_present':len(state['profiles'])>=12,
                'population_budget':all(sum(s['counts'].values())<=120 for s in state['samples']),
                'no_sampled_terrain_penetration':all(s.get('clearance_cm',100000)>=s.get('radius_cm',0) for s in state['air_support']),
                'stream_frames_advance':ptz.stream.frame_number-state['frames']>1000,
                'stream_connected':ptz.stream.is_connected(),
                'main_camera_confirmed_looking_away':state.get('away_observed',False),
                'ground_still_moves_after_camera_away':all(any(s['elapsed']>65 and s['moving'].get(str(k),0)>0 for s in state['samples']) for k in (unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER,unreal.LivingKind.CAR))}
            result={**state,'started_unix':started_utc,'checks':checks,'profiles':sorted(state['profiles']),'ground_moving':sorted(state['ground_moving']),'air_moving':sorted(state['air_moving'])}
            result['passed']=all(checks.values())
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-population.json').write_text(json.dumps(result,indent=2))
            print('MAIN_POPULATION',checks)
        except Exception as error:
            unreal.unregister_slate_post_tick_callback(handle)
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-population-error.txt').write_text(repr(error))
            raise
    handle=unreal.register_slate_post_tick_callback(tick)
    globals()['main_population_handle']=handle
    print('MAIN_ACCEPTANCE_STARTED')

run_main_acceptance()
