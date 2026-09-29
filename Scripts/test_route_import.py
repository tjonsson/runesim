"""Editor integration test: WGS84 conversion, direction flags and repeat imports.

Creates and removes only uniquely tagged test candidates; saves no map.
"""
import unreal
import os
import json
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert 'LivingWorldDemo' in world.get_name()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
georef=unreal.CesiumGeoreference.get_default_georeference(world)
original={a.get_path_name() for a in actors.get_all_level_actors()}
fixture=Path(unreal.Paths.project_saved_dir())/'LivingWorld/route-import-fixture.geojson'
points=[unreal.Vector(3000,0,0),unreal.Vector(3400,0,0),unreal.Vector(3400,400,0)]
coordinates=[]
for point in points:
    llh=georef.transform_unreal_position_to_longitude_latitude_height(point)
    coordinates.append([llh.x,llh.y,llh.z])
features=[]
for name,closed in [('OneWayImportTest',False),('ClosedImportTest',True)]:
    features.append({'type':'Feature','properties':{'name':name,'mode':'drive','source':'RuneSim synthetic import integration test',
        'license':'synthetic test fixture','attribution':'RuneSim','one_way':True},
        'geometry':{'type':'LineString','coordinates':coordinates+([coordinates[0]] if closed else [])}})
fixture.write_text(json.dumps({'type':'FeatureCollection','features':features}))
saved=os.environ.get('RUNESIM_ROUTES_GEOJSON')
report={}
try:
    os.environ['RUNESIM_ROUTES_GEOJSON']=str(fixture.resolve())
    code=(Path(unreal.Paths.project_dir())/'Scripts/import_routes_geojson.py').read_text(encoding='utf-8')
    exec(compile(code,'import_routes_geojson.py','exec'),{})
    imported=[a for a in actors.get_all_level_actors() if a.get_path_name() not in original and isinstance(a,unreal.LivingRoute)]
    assert len(imported)==2,len(imported)
    report['unvalidated']=all(not a.get_editor_property('validated') for a in imported)
    report['one_way_preserved']=all(a.get_editor_property('one_way') for a in imported)
    report['loop_preserved']=sum(a.path.is_closed_loop() for a in imported)==1
    report['coordinate_roundtrip']=all((a.path.get_location_at_spline_point(0,unreal.SplineCoordinateSpace.WORLD)-points[0]).length()<1 for a in imported)
    exec(compile(code,'import_routes_geojson.py','exec'),{})
    repeated=[a for a in actors.get_all_level_actors() if a.get_path_name() not in original and isinstance(a,unreal.LivingRoute)]
    report['repeat_skips_existing']=len(repeated)==2
    report['passed']=all(report.values())
    assert report['passed'],report
finally:
    for a in actors.get_all_level_actors():
        if a.get_path_name() not in original and isinstance(a,unreal.LivingRoute):actors.destroy_actor(a)
    if saved is None:os.environ.pop('RUNESIM_ROUTES_GEOJSON',None)
    else:os.environ['RUNESIM_ROUTES_GEOJSON']=saved
    (fixture.parent/'route-import-test.json').write_text(json.dumps(report,indent=2))
print('ROUTE_IMPORT_RESULT',report)
