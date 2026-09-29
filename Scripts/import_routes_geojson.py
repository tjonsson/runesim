"""Unreal script: import a reviewed GeoJSON candidate network without enabling it.

Set RUNESIM_ROUTES_GEOJSON to a local file containing WGS84 LineStrings.
Properties: mode=walk|drive, source, license, attribution. Height is ellipsoidal metres.
Imported routes remain unvalidated until an operator inspects terrain, buildings and water.
"""
import json
import math
import os
import hashlib
from pathlib import Path
import unreal

source=Path(os.environ.get('RUNESIM_ROUTES_GEOJSON',str(Path(unreal.Paths.project_saved_dir())/'LivingWorld/routes.geojson'))).resolve()
data=json.loads(source.read_text())
if data.get('type')!='FeatureCollection': raise ValueError('Expected FeatureCollection')
if unreal.EditorLevelLibrary.get_pie_worlds(False):raise RuntimeError('Stop PIE before importing route candidates')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
georef=unreal.CesiumGeoreference.get_default_georeference(world)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
prepared=[]
for feature in data['features']:
    props=feature.get('properties',{}); geometry=feature.get('geometry',{})
    if geometry.get('type')!='LineString' or props.get('mode') not in ['walk','drive']: continue
    if not all(props.get(k) for k in ['source','license','attribution']): raise ValueError('Route requires provenance')
    coordinates=geometry['coordinates']
    if len(coordinates)<2 or len(coordinates)>10000: raise ValueError('Invalid route length')
    if any(len(p)!=3 or not all(math.isfinite(v) for v in p) or abs(p[0])>180 or abs(p[1])>90 for p in coordinates):
        raise ValueError('Expected finite [longitude,latitude,ellipsoid-height] coordinates')
    if not isinstance(props.get('one_way',False),bool):raise ValueError('one_way must be a boolean')
    closed=len(coordinates)>3 and coordinates[0]==coordinates[-1]
    if closed:coordinates=coordinates[:-1]
    # Re-running an import cannot duplicate or overwrite an operator-reviewed route.
    identity=json.dumps([props['source'],props['mode'],props.get('osm_way'),coordinates,closed,props.get('one_way',False)],sort_keys=True)
    tag='LivingWorld.Import.'+hashlib.sha256(identity.encode()).hexdigest()[:20]
    prepared.append((props,[georef.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*p)) for p in coordinates],closed,tag))
existing={str(tag) for a in actors.get_all_level_actors() if isinstance(a,unreal.LivingRoute) for tag in a.tags}
created=[]
with unreal.ScopedEditorTransaction('Import unvalidated Living World routes'):
    try:
        for props,points,closed,tag in prepared:
            if tag in existing:continue
            route=actors.spawn_actor_from_class(unreal.LivingRoute,points[0])
            if not route:raise RuntimeError('Could not create route')
            created.append(route)
            route.set_actor_label('Candidate '+props.get('name',props['mode']))
            route.path.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD)
            for i in range(len(points)): route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
            route.path.set_closed_loop(closed)
            route.set_editor_property('vehicles',props['mode']=='drive')
            route.set_editor_property('one_way',props.get('one_way',False))
            route.set_editor_property('validated',False)
            route.set_editor_property('provenance',json.dumps(props,ensure_ascii=False))
            route.tags=[tag]
            existing.add(tag)
    except Exception:
        for route in created:actors.destroy_actor(route)
        raise
print('Imported',len(created),'candidate routes. Existing candidates retained. Review in editor before validating; map is not auto-saved.')
