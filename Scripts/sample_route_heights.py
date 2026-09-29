"""Query real terrain heights for three public GIS candidates in an empty editor world.

Uses Cesium World Terrain (asset 1) and the project's existing Cesium connection.
The asynchronous result is a local review artifact; routes stay unvalidated.
"""
import unreal,json,math,time
from pathlib import Path

def sample_route_heights():
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_name()=='Entry','Run in an empty Entry world, not the user scenario'
    root=Path(unreal.Paths.project_dir())
    data=json.loads((root/'Samples/LivingWorld/OSM/golden-gate-park.candidates.geojson').read_text(encoding='utf-8'))
    selected=[f for f in data['features'] if f['properties']['osm_way'] in (8921602,444234267,894943847)]
    assert len(selected)==3
    positions=[];counts=[]
    for feature in selected:
        points=[];source=feature['geometry']['coordinates']
        for a,b in zip(source,source[1:]):
            metres=math.hypot((b[0]-a[0])*111320*math.cos(math.radians(a[1])),(b[1]-a[1])*110540)
            count=max(1,math.ceil(metres/5.))
            for i in range(count):points.append([a[0]+(b[0]-a[0])*i/count,a[1]+(b[1]-a[1])*i/count,0.])
        points.append(source[-1]);counts.append(len(points))
        positions.extend(unreal.Vector(*p) for p in points)
    assert len(positions)<=1000
    actor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.Cesium3DTileset,unreal.Vector(),transient=True)
    actor.set_actor_label('LivingReview_TerrainHeightQuery')
    actor.set_editor_property('ion_asset_id',1)
    state={'actor':actor,'started':time.monotonic()}
    def complete(results,warnings):
        success=len(results)==len(positions) and all(r.sample_success for r in results)
        output=root/'Saved/LivingWorld/Routes';output.mkdir(parents=True,exist_ok=True)
        report={'passed':success,'samples':len(results),'successful':sum(r.sample_success for r in results),'warnings':list(warnings),
                'source':'Cesium World Terrain via the existing project Cesium connection','elapsed_seconds':time.monotonic()-state['started'],
                'osm_ways':[f['properties']['osm_way'] for f in selected],'validated':False}
        if success:
            offset=0
            for feature,count in zip(selected,counts):
                feature['geometry']['coordinates']=[[r.longitude_latitude_height.x,r.longitude_latitude_height.y,r.longitude_latitude_height.z] for r in results[offset:offset+count]]
                feature['properties']['height_status']='Cesium World Terrain most-detailed samples at <=5 m spacing; collision/alignment review still required'
                offset+=count
            (output/'terrain-candidates.geojson').write_text(json.dumps({'type':'FeatureCollection','features':selected},indent=2),encoding='utf-8')
            report['height_range_m']=[min(r.longitude_latitude_height.z for r in results),max(r.longitude_latitude_height.z for r in results)]
        (output/'height-query.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print('CESIUM_HEIGHT_QUERY',report)
        # Keep the transient actor alive until the editor/review closes. Destroying
        # a tileset from inside its completion delegate invalidates Cesium's stack.
    action=unreal.CesiumSampleHeightMostDetailedAsyncAction.get_default_object().call_method('SampleHeightMostDetailed',args=(actor,positions))
    action.on_heights_sampled.add_callable(complete)
    state['action']=action;state['callback']=complete
    action.call_method('Activate')
    print('CESIUM_HEIGHT_QUERY_STARTED',len(positions))
    return state

route_height_query=sample_route_heights()
