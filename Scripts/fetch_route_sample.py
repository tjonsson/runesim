"""Fetch a small public Golden Gate Park GIS fixture, never the user's scene origin.

Network access occurs only when explicitly running this script. Data remains
unvalidated; the zero-metre ellipsoidal height is a placeholder for terrain review.
"""
import json,hashlib,urllib.request,urllib.parse
from datetime import datetime,timezone
from pathlib import Path
from osm_route_candidates import convert

def main():
    folder=Path(__file__).resolve().parents[1]/'Samples/LivingWorld/OSM'
    folder.mkdir(parents=True,exist_ok=True)
    bbox='37.767,-122.470,37.772,-122.462'
    query='[out:json][timeout:25];('+''.join('nwr['+tag+']('+bbox+');' for tag in ('highway','building','natural=water','natural=wetland','water','waterway','landuse=reservoir','landuse=basin','barrier'))+');out geom;'
    endpoint='https://overpass-api.de/api/interpreter'
    request=urllib.request.Request(endpoint,data=urllib.parse.urlencode({'data':query}).encode(),headers={'User-Agent':'RuneSim-route-fixture/1.0','Content-Type':'application/x-www-form-urlencoded'})
    with urllib.request.urlopen(request,timeout=45) as response:raw=response.read(16*1024*1024+1)
    if len(raw)>16*1024*1024:raise ValueError('Unexpectedly large export')
    data=json.loads(raw)
    if data.get('remark'):raise ValueError('Incomplete Overpass result: '+data['remark'])
    candidates,rejected=convert(data,0.)
    (folder/'golden-gate-park.overpass.json').write_bytes(raw)
    (folder/'golden-gate-park.candidates.geojson').write_text(json.dumps(candidates,indent=2,ensure_ascii=False),encoding='utf-8')
    counts={mode:sum(f['properties']['mode']==mode for f in candidates['features']) for mode in ('walk','drive')}
    report={'downloaded_utc':datetime.now(timezone.utc).isoformat(),'endpoint':endpoint,'query':query,'bbox_south_west_north_east':bbox,
            'sha256':hashlib.sha256(raw).hexdigest(),'osm_timestamp':data.get('osm3s',{}).get('timestamp_osm_base'),
            'license':'ODbL-1.0','attribution':'© OpenStreetMap contributors; https://www.openstreetmap.org/copyright',
            'element_count':len(data['elements']),'candidate_counts':counts,'validated':False,'rejected':rejected}
    (folder/'golden-gate-park.review.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('query','rejected')}))

if __name__=='__main__':main()
