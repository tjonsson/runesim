"""Create two unvalidated simulation corridors on mapped tracks in MainLevel.

This is an explicit scene-authoring step, not automatic OSM access approval.
The tracks need visual, width and full-profile runtime review before validation.
"""
import unreal,json,math
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors=sub.get_all_level_actors()
assert not any('LivingWorld.MainCorridor' in [str(t) for t in a.tags] for a in all_actors),'Main corridors already exist'
geo=unreal.CesiumGeoreference.get_default_georeference(world)
tile=next(a for a in all_actors if isinstance(a,unreal.Cesium3DTileset))
rotation=tile.get_actor_rotation();scale=tile.get_actor_scale3d()
assert all(abs(v)<0.001 for v in (rotation.pitch,rotation.yaw,rotation.roll)), 'Rotated tiles require a transformed source alignment'
assert all(abs(v-1)<0.001 for v in (scale.x,scale.y,scale.z)), 'Scaled tiles require a transformed source alignment'
ignored=[a for a in all_actors if not isinstance(a,unreal.Cesium3DTileset)]
root=Path(unreal.Paths.project_dir());folder=root/'Saved/LivingWorld/MainRoutes'
source=json.loads((folder/'main.overpass.json').read_text())
created=[];reports=[]
for way_id,vehicles,half_length in ((1047212312,True,10000),(946002495,False,6500)):
    way=next(e for e in source['elements'] if e['id']==way_id)
    points=[geo.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(p['lon'],p['lat'],geo.get_editor_property('origin_height')))+tile.get_actor_location() for p in way['geometry']]
    nearest=min(range(len(points)),key=lambda i:math.hypot(points[i].x,points[i].y))
    first=last=nearest;distance=0
    while first>0 and distance<half_length:
        distance+=(points[first]-points[first-1]).length();first-=1
    distance=0
    while last<len(points)-1 and distance<half_length:
        distance+=(points[last+1]-points[last]).length();last+=1
    samples=[]
    for a,b in zip(points[first:last],points[first+1:last+1]):
        count=max(1,math.ceil((b-a).length()/200))
        samples.extend(a+(b-a)*(i/count) for i in range(count))
    samples.append(points[last]);ground=[];normals=[]
    for point in samples:
        hit=unreal.SystemLibrary.line_trace_single_for_objects(world,unreal.Vector(point.x,point.y,100000),unreal.Vector(point.x,point.y,-200000),[unreal.ObjectTypeQuery.ECC_WORLD_STATIC],True,ignored,unreal.DrawDebugTrace.NONE)
        assert hit,'Terrain not loaded'
        fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
        assert fields[0] and isinstance(fields[9],unreal.Cesium3DTileset),'Expected actual Cesium collision'
        ground.append(fields[5]);normals.append(fields[7].z)
    route=sub.spawn_actor_from_class(unreal.LivingRoute,ground[0])
    route.set_actor_label('Living Main '+('Vehicle track' if vehicles else 'Walking track')+' candidate')
    route.tags=['LivingWorld.MainCorridor']
    route.path.set_spline_points(ground,unreal.SplineCoordinateSpace.WORLD)
    for i in range(len(ground)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
    route.vehicles=vehicles;route.one_way=vehicles;route.validated=False
    route.reviewed_half_width_cm=0
    route.provenance=json.dumps({'source':f'https://www.openstreetmap.org/way/{way_id}','license':'ODbL-1.0','attribution':'© OpenStreetMap contributors; https://www.openstreetmap.org/copyright','osm_tags':way['tags'],'scope':'Authored local simulation corridor on a mapped track; no public/legal access determination','review_status':'candidate, no spawning','tile_translation_cm':[tile.get_actor_location().x,tile.get_actor_location().y,tile.get_actor_location().z]})
    created.append(route)
    for a,b in zip(ground,ground[1:]):unreal.SystemLibrary.draw_debug_line(world,a+unreal.Vector(0,0,30),b+unreal.Vector(0,0,30),unreal.LinearColor(1,0.15 if vehicles else 1,0,1),300,10)
    reports.append({'way':way_id,'vehicles':vehicles,'samples':len(ground),'length_cm':route.path.get_spline_length(),'minimum_normal_z':min(normals),'world_points':[[p.x,p.y,p.z] for p in ground]})
(folder/'corridor-candidates.json').write_text(json.dumps(reports,indent=2))
sub.set_selected_level_actors(created)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,-11000,18000),unreal.Rotator(pitch=-90,yaw=0))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('MAIN_CORRIDOR_CANDIDATES',[(r['vehicles'],r['samples'],r['length_cm'],r['minimum_normal_z']) for r in reports])
