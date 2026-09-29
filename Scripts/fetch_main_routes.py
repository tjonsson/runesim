"""Fetch OSM geometry around the inspected MainLevel origin for local review."""
import json,hashlib,urllib.request,urllib.parse
from datetime import datetime,timezone
from pathlib import Path
from osm_route_candidates import convert

root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'Saved/LivingWorld/main-inventory.json').read_text())
lon,lat,height=next(a['origin'] for a in inventory['actors'] if 'origin' in a)
bbox=f'{lat-.006},{lon-.008},{lat+.006},{lon+.008}'
query='[out:json][timeout:25];('+''.join('nwr['+tag+']('+bbox+');' for tag in ('highway','building','natural=water','natural=wetland','water','waterway','landuse=reservoir','barrier'))+');out geom;'
endpoint='https://overpass-api.de/api/interpreter'
request=urllib.request.Request(endpoint,data=urllib.parse.urlencode({'data':query}).encode(),headers={'User-Agent':'RuneSim-main-route-review/1.0','Content-Type':'application/x-www-form-urlencoded'})
with urllib.request.urlopen(request,timeout=45) as response:raw=response.read(16*1024*1024+1)
assert len(raw)<=16*1024*1024
data=json.loads(raw)
if data.get('remark'):raise RuntimeError(data['remark'])
candidates,rejected=convert(data,height)
folder=root/'Saved/LivingWorld/MainRoutes';folder.mkdir(exist_ok=True)
(folder/'main.overpass.json').write_bytes(raw)
(folder/'main.candidates.geojson').write_text(json.dumps(candidates,indent=2),encoding='utf-8')
report={'downloaded_utc':datetime.now(timezone.utc).isoformat(),'source':endpoint,'query':query,'sha256':hashlib.sha256(raw).hexdigest(),'license':'ODbL-1.0','attribution':'© OpenStreetMap contributors; https://www.openstreetmap.org/copyright','elements':len(data['elements']),'candidate_count':len(candidates['features']),'rejected':rejected,'validated':False}
(folder/'main.review.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print({k:v for k,v in report.items() if k not in ('query','rejected')})
print([(f['properties']['osm_way'],f['properties']['mode'],f['properties']['name']) for f in candidates['features']])
