"""Check real terrain collision along sampled public OSM candidates.

Creates an isolated review map, keeps every route unvalidated, and checks terrain
retention while the ordinary editor camera looks away. Does not change MainLevel.
"""
import unreal,json,time,math,os
from pathlib import Path

def run_cesium_collision_review():
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
    root=Path(unreal.Paths.project_dir());folder=root/'Saved/LivingWorld/Routes'
    data=json.loads((folder/'terrain-candidates.geojson').read_text())
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    path='/Game/LivingWorld/Maps/CesiumRouteReview'
    assert not unreal.EditorAssetLibrary.does_asset_exist(path),'Review map exists; inspect it before replacing anything'
    assert levels.new_level(path)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    georef=unreal.CesiumGeoreference.get_default_georeference(world)
    coordinates=[p for f in data['features'] for p in f['geometry']['coordinates']]
    origin=unreal.Vector(sum(p[0] for p in coordinates)/len(coordinates),sum(p[1] for p in coordinates)/len(coordinates),42.)
    georef.set_origin_longitude_latitude_height(origin)
    terrain=actors.spawn_actor_from_class(unreal.Cesium3DTileset,unreal.Vector())
    terrain.set_actor_label('Cesium World Terrain - route review')
    terrain.set_editor_property('ion_asset_id',1)
    terrain.set_editor_property('create_physics_meshes',True)
    terrain.set_editor_property('maximum_screen_space_error',2.)
    manager=unreal.CesiumCameraManager.get_default_camera_manager(world)
    camera=unreal.CesiumCamera();camera.location=unreal.Vector(0,0,40000)
    camera.rotation=unreal.Rotator(pitch=-90);camera.viewport_size=unreal.Vector2D(1920,1080);camera.field_of_view_degrees=65.
    manager.additional_cameras=[camera]
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,20000),unreal.Rotator(pitch=-90))
    previous=os.environ.get('RUNESIM_ROUTES_GEOJSON')
    os.environ['RUNESIM_ROUTES_GEOJSON']=str(folder/'terrain-candidates.geojson')
    try:exec(compile((root/'Scripts/import_routes_geojson.py').read_text(),str(root/'Scripts/import_routes_geojson.py'),'exec'),{})
    finally:
        if previous is None:os.environ.pop('RUNESIM_ROUTES_GEOJSON',None)
        else:os.environ['RUNESIM_ROUTES_GEOJSON']=previous
    points=[georef.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*p)) for p in coordinates]
    state={'phase':'load','start':time.monotonic(),'last':0,'samples':[]}
    def trace():
        hits=[]
        for point in points:
            hit=unreal.SystemLibrary.line_trace_single_for_objects(world,point+unreal.Vector(0,0,150),point-unreal.Vector(0,0,150),[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],True,[],unreal.DrawDebugTrace.NONE)
            if hit:
                fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
                hits.append({'hit':bool(fields[0]),'deviation_cm':(fields[5]-point).length(),'normal_z':fields[7].z})
            else:hits.append({'hit':False})
        return hits
    def frame(dt):
        now=time.monotonic()
        if now-state['last']<2:return
        state['last']=now
        try:
            hits=trace();passed=all(h['hit'] and h['normal_z']>=math.cos(math.radians(30)) for h in hits)
            if state['phase']=='load' and not passed and now-state['start']<90:return
            state['samples'].append({'phase':state['phase'],'passed':passed,'points':hits})
            if state['phase']=='load' and passed:
                unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,20000),unreal.Rotator(pitch=80,yaw=180))
                state['phase']='camera_turned_away';state['start']=now;return
            if state['phase']=='camera_turned_away' and now-state['start']<20:
                state['samples'].pop();return
            unreal.unregister_slate_post_tick_callback(state['handle'])
            report={'passed':all(s['passed'] for s in state['samples']),'samples':len(points),'phases':state['samples'],'routes_validated':False,
                    'note':'Terrain collision/retention only. Photogrammetry alignment, barriers and road rules still require review.'}
            (folder/'cesium-collision.json').write_text(json.dumps(report,indent=2));levels.save_current_level();print('CESIUM_COLLISION_REVIEW',report['passed'],len(points))
        except Exception as error:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            (folder/'cesium-collision.json').write_text(json.dumps({'passed':False,'error':str(error)},indent=2));raise
    state['handle']=unreal.register_slate_post_tick_callback(frame)
    return state

cesium_collision_review=run_cesium_collision_review()
