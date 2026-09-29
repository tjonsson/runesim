"""Conservative exclusion envelopes for locally exported OSM geometry.

Envelopes deliberately include polygon holes and relation gaps. False exclusions
are preferable to approving a corridor through incomplete building/water data.
This is a prefilter, not terrain, topology, or legal-access validation.
"""
import math

def excluded_kind(tags):
    if tags.get('building','no')!='no': return 'building'
    if tags.get('natural') in ('water','wetland') or tags.get('water') or tags.get('waterway') or tags.get('landuse') in ('reservoir','basin'):
        return 'water'
    if tags.get('barrier','no')!='no': return 'barrier'
    return None

def envelopes(data):
    output=[]
    for element in data.get('elements',[]):
        kind=excluded_kind(element.get('tags',{}))
        if not kind: continue
        points=list(element.get('geometry',[]))
        for member in element.get('members',[]):points.extend(member.get('geometry',[]))
        if element.get('type')=='node':points=[element]
        if not points or any(not all(isinstance(p.get(k),(int,float)) and not isinstance(p.get(k),bool) and math.isfinite(p[k]) for k in ('lon','lat')) or abs(p['lon'])>180 or abs(p['lat'])>90 for p in points):
            raise ValueError('Incomplete exclusion geometry; export buildings, water and barriers with out geom')
        xs=[p['lon'] for p in points];ys=[p['lat'] for p in points]
        if max(xs)-min(xs)>180:raise ValueError('Antimeridian exclusions require a GIS projection')
        output.append({'kind':kind,'id':element.get('id'),'bounds':(min(xs),min(ys),max(xs),max(ys))})
    return output

def intersects_box(a,b,box):
    """Segment/closed rectangle slab test, including touching its boundary."""
    low,high=0.,1.
    for i in (0,1):
        delta=b[i]-a[i]
        if abs(delta)<1e-15:
            if a[i]<box[i] or a[i]>box[i+2]:return False
        else:
            near,far=sorted(((box[i]-a[i])/delta,(box[i+2]-a[i])/delta))
            low=max(low,near);high=min(high,far)
            if low>high:return False
    return True

def first_conflict(coordinates,exclusions,clearance_m):
    latitude=sum(p[1] for p in coordinates)/len(coordinates)
    dx=clearance_m/(111320*max(.01,math.cos(math.radians(latitude))))
    dy=clearance_m/110540
    for item in exclusions:
        x0,y0,x1,y1=item['bounds'];box=(x0-dx,y0-dy,x1+dx,y1+dy)
        if any(intersects_box(a,b,box) for a,b in zip(coordinates,coordinates[1:])):return item
    return None
