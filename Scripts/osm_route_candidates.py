"""Convert a local Overpass `out geom` JSON export into unvalidated GeoJSON.

Deliberately conservative: no roadway-as-sidewalk inference, no conditional
access evaluation, no bridge/tunnel/area routing, and no automatic approval.
The explicit height is only an initial ellipsoidal estimate for editor review.
"""
import argparse
import json
import math
from pathlib import Path
from osm_exclusions import envelopes,first_conflict

ALLOW={'yes','designated','permissive'}
WALK={'footway','pedestrian','path'}
DRIVE={'residential','unclassified','tertiary','secondary','primary','service','living_street'}

def direction_for(tags,mode):
    if mode=='walk':return tags.get('oneway:foot','no')
    return tags.get('oneway:motorcar',tags.get('oneway:motor_vehicle',tags.get('oneway','yes' if tags.get('junction')=='roundabout' else 'no')))

def classify(tags,mode):
    highway=tags.get('highway','')
    if any(':conditional' in k for k in tags):return 'conditional restriction requires review'
    if any(k.endswith((':forward',':backward')) for k in tags):return 'directional restriction requires authored routing'
    if any(tags.get(k,'no') not in ('no','0','') for k in ('bridge','tunnel','covered','indoor','ford','area')):
        return 'structure, ford or area requires authored routing'
    if tags.get('layer','0')!='0' or tags.get('level','0')!='0':return 'non-ground level requires review'
    if tags.get('construction') or tags.get('proposed') or tags.get('disused')=='yes':return 'inactive or construction way'
    if tags.get('building','no')!='no' or tags.get('natural')=='water' or tags.get('water'):
        return 'building or water geometry requires review'
    if mode=='walk':
        if highway not in WALK:return 'not a separately mapped walking way'
        keys=('access','foot')
    else:
        if highway not in DRIVE:return 'unsupported driving way'
        keys=('access','vehicle','motor_vehicle','motorcar')
    # More specific mode tags override general restrictions in OSM.
    access=next((tags[k] for k in reversed(keys) if k in tags),'yes')
    if access not in ALLOW:return 'restricted or unknown '+mode+' access: '+access
    oneway=direction_for(tags,mode)
    if oneway not in ('no','0','false','yes','1','true','-1'):return 'ambiguous one-way restriction'
    return None

def convert(data,height):
    if not math.isfinite(height):raise ValueError('Height must be finite ellipsoidal metres')
    features=[];rejected=[]
    exclusions=envelopes(data)
    for way in data.get('elements',[]):
        if way.get('type')!='way':continue
        tags=way.get('tags',{})
        geometry=way.get('geometry',[])
        coordinates=[]
        valid=2<=len(geometry)<=10000
        for point in geometry:
            lon,lat=point.get('lon'),point.get('lat')
            if not all(isinstance(v,(int,float)) and not isinstance(v,bool) and math.isfinite(v) for v in (lon,lat)) or abs(lon)>180 or abs(lat)>90:
                valid=False;break
            if not coordinates or coordinates[-1][:2]!=[lon,lat]:coordinates.append([lon,lat,height])
        valid=valid and len(coordinates)>=2
        for mode in ('walk','drive'):
            reason=classify(tags,mode) if valid else 'missing or invalid geometry'
            if reason is None:
                conflict=first_conflict(coordinates,exclusions,3. if mode=='drive' else 1.)
                if conflict:reason='intersects supplied '+conflict['kind']+' exclusion '+str(conflict['id'])
            if reason:
                rejected.append({'osm_way':way.get('id'),'mode':mode,'reason':reason});continue
            direction=direction_for(tags,mode)
            points=list(reversed(coordinates)) if direction=='-1' else list(coordinates)
            properties={'mode':mode,'name':tags.get('name','OSM way '+str(way['id'])),
                'osm_way':way['id'],'source':'https://www.openstreetmap.org/way/'+str(way['id']),
                'license':'ODbL-1.0','attribution':'© OpenStreetMap contributors; https://www.openstreetmap.org/copyright',
                'one_way':direction in ('yes','1','true','-1'),'validated':False,
                'height_status':'uniform estimate; requires terrain/surface review',
                'exclusion_prefilter':{'supplied_envelopes':len(exclusions),'clearance_m':3. if mode=='drive' else 1.,'automatic_approval':False},
                'review_required':['height','buildings','water','crossings','width','barriers','turn restrictions','local access rules'],
                'osm_tags':tags}
            features.append({'type':'Feature','properties':properties,
                             'geometry':{'type':'LineString','coordinates':points}})
    return {'type':'FeatureCollection','features':features},rejected

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--ellipsoid-height',type=float,required=True)
    args=parser.parse_args()
    data,rejected=convert(json.loads(args.input.read_text(encoding='utf-8')),args.ellipsoid_height)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    args.output.with_suffix('.review.json').write_text(json.dumps({'candidates':len(data['features']),'rejected':rejected},indent=2),encoding='utf-8')
    print(f"Prepared {len(data['features'])} unvalidated candidates; {len(rejected)} mode exclusions")

if __name__=='__main__':main()
