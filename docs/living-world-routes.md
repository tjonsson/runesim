# Reviewed routes from OpenStreetMap

`Scripts/osm_route_candidates.py` converts a **local Overpass JSON export containing way geometry** into candidate walking/driving GeoJSON. It does not assert that an OSM way is safe, legal, connected or accurately aligned to Cesium imagery.

Include buildings, water and barriers in that export. The converter rejects route segments intersecting their conservative bounding envelopes, expanded by 1 m for walking and 3 m for driving. It checks the complete segment, so outside endpoints cannot conceal a crossing. Relations include holes and gaps in the exclusion envelope. Incomplete exclusion geometry rejects the export. Absence of an exclusion in the input is not evidence that an area is clear; candidates always remain unvalidated.

A real public-data fixture is included in `Samples/LivingWorld/OSM`: 687 Golden Gate Park elements, 175 walking candidates and 11 driving candidates after prefiltering. It remains independent of MainLevel and uses placeholder heights. See that directory's README for provenance and limitations. Nine offline policy/geometry tests cover access precedence, incomplete data, boundary contact, segment crossings and clearance.

## Real Cesium height and collision test

`sample_route_heights.py` samples three fixture candidates through the installed Cesium plugin's most-detailed terrain query, using the project's existing Cesium connection and Cesium World Terrain. Run it in the empty `/Engine/Maps/Entry` editor world. It densifies the selected centre lines to at most 5 m spacing and writes local review artifacts under `Saved/LivingWorld/Routes`. It does not query or modify the user's main scenario location.

All 55 height samples succeeded with no Cesium warnings (41.55–43.06 m above the ellipsoid). `test_cesium_route_collision.py` then created `/Game/LivingWorld/Maps/CesiumRouteReview`, imported the three candidates, and confirmed static terrain collision and acceptable surface normals at all 55 locations. All points remained available after the ordinary editor camera looked away for 20 seconds; a bounded additional selection camera retained the area. The test ran in a separate NullRHI editor while the packaged Pi demo kept streaming.

The first height harness destroyed its tileset inside Cesium's completion delegate and crashed that editor after returning the results. The harness now retains the transient actor until editor teardown. A fresh-process repeat and the collision test passed with the fix. No runtime/packaged actor used that cleanup path.

The review map's routes **remain unvalidated**. This verifies DEM height sampling and collision retention, not photogrammetry alignment, physical road width, bus/pedestrian coexistence, crossings, legal access, or missing map obstacles. In particular, two selected paths permit public transport. The review map is an editor artifact and was created after the packaged flat-scene build. MainLevel subsequently received two separately authored and locally reviewed corridors; see [MainLevel route validation](living-world-cesium.md). Those corridors do not approve the OSM fixture or establish a citywide ground network.

```powershell
python Scripts/osm_route_candidates.py path/to/overpass.json Saved/LivingWorld/routes.geojson --ellipsoid-height 25
python -m unittest discover -s Scripts/tests -v
python Scripts/ue_remote.py Scripts/import_routes_geojson.py
```

Replace 25 with an appropriate initial **ellipsoidal** height estimate; this is not height above ground or sea level. Every imported candidate stays unvalidated. Stop PIE first. The default import file is `Saved/LivingWorld/routes.geojson`; the editor process can override it through `RUNESIM_ROUTES_GEOJSON`. Import is undoable and does not save the map. Repeating an identical import skips existing candidates, including already reviewed routes.

Only separately mapped footways, pedestrian ways and paths become walking candidates. A `sidewalk=both` tag on a road does not supply sidewalk geometry, so it cannot place pedestrians along that road centreline. The converter preserves the complete tags and attribution, honours mode-specific access overrides and reverses geometry for `oneway=-1`. Supported one-way roads retire agents at an open route's exit; closed one-way loops keep circulating. See OSM's [access](https://wiki.openstreetmap.org/wiki/Key:access), [sidewalk](https://wiki.openstreetmap.org/wiki/Key:sidewalk), and [one-way](https://wiki.openstreetmap.org/wiki/Key:oneway) documentation.

Conditional or directional restrictions, bridges, tunnels, fords, indoor ways, areas, non-ground levels and unsupported road types are excluded with reasons in the adjacent `.review.json`. This is a conservative simulation preparation policy, not a complete OSM router or a legal-access determination. Barrier nodes, traffic lights, turn-restriction relations and region-specific defaults still need review.

Before setting a route's `Validated` flag, review the entire corridor in loaded Cesium terrain: replace estimated heights with the intended road/footpath surface; check buildings, water, crossings, width, slopes, barriers and access. Runtime ground traces and collision sweeps still stop movement when geometry is absent or obstructed. They cannot infer water/building semantics from photogrammetry alone. Keep multi-level crossings and lane turns as authored geometry until they have been explicitly tested.

OSM-derived data retains “© OpenStreetMap contributors” and its [ODbL attribution link](https://www.openstreetmap.org/copyright). No OSM-derived network has been approved for MainLevel yet.

MainLevel's corridors were later extended from terrain scans rather than OSM; see [extended reviewed corridors](living-world-cesium.md#extended-reviewed-corridors--september-30).
