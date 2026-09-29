# Golden Gate Park route fixture

This small public GIS fixture exercises the route preparation pipeline independently of the user's Cesium scene. It covers the rectangle 37.767,-122.470 to 37.772,-122.462 (south/west to north/east). It does not change MainLevel or enable any ground agents.

Data: © OpenStreetMap contributors, available under [ODbL 1.0](https://www.openstreetmap.org/copyright). Preserve attribution and the ODbL database terms when redistributing this fixture or derived databases. `golden-gate-park.review.json` records the source timestamp, query, checksum, candidate counts and exclusions. `golden-gate-park.overpass.json` is the original export; `golden-gate-park.candidates.geojson` is the derived candidate database, also ODbL 1.0.

The September 29 export contains 687 elements and produced 175 walking and 11 driving candidates after conservative tag and building/water/barrier-envelope filtering. These are **not approved routes**. Coordinates use a placeholder ellipsoidal height of zero. Terrain heights, corridor widths, boundary coverage, connectivity, crossings, access and missing map features still require review. Large or multipart exclusions can deliberately remove valid paths; the prefilter favors omission over an unsafe assumption.

Reproduce with `python Scripts/fetch_route_sample.py`. This explicitly downloads a new export from the public Overpass service; normal simulator startup makes no OSM requests. Query syntax follows the [Overpass QL reference](https://wiki.openstreetmap.org/wiki/Overpass_API/Overpass_QL).
