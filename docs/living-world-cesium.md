# Living World in MainLevel

The current package also includes the later [streamed-camera image correction](camera-image-quality.md), with its own visual review and playback measurements. Results below describe the preceding MainLevel integration build.

The production scene remains `/Game/MainLevel`, using its existing Google Photorealistic 3D Tiles, Cesium georeference and AirSim game mode. The original tileset translation, georeference origin and telemetry actors are preserved. The pre-integration map is backed up at `Saved/LivingWorld/BeforeMainIntegration/MainLevel.umap`.

Flight homes are stored in Earth-centred coordinates. Guidance, altitude, banking and the population's streaming camera use Cesium's local east/south/up frame. Air agents require rendered terrain support before spawning and check clearance ahead and below during flight. Missing collision pauses flight; a raised terrain tile triggers a climb. These are ambient kinematic actors, not an aerodynamic flight model. The dedicated overhead Cesium camera includes the aircraft turn-radius envelope, so camera direction does not determine the whole population's tile coverage. The API follows [CesiumGeoreference](https://cesium.com/learn/cesium-unreal/ref-doc/classACesiumGeoreference.html).

Two short ground corridors were authored on the visible dirt road. OSM track candidates were rejected after visual comparison: they crossed a gully and vegetation in the rendered scene. The replacement uses actual tile collision and is limited to this reviewed area. It does not enable a city-wide road network or make a public-access determination. Candidate source attribution and rejection records remain in `Saved/LivingWorld/MainRoutes`.

The final vehicle corridor is approximately 32 metres long and 2.5 metres wide, shifted 75 cm within the original reviewed road strip. A centimetre-spaced scan passed 2,599 complete jeep placements, including chassis and all four wheel checks, between the entry/exit margins. Earlier 102 m and 55 m candidates contained isolated unsupported terrain facets and were shortened. The pedestrian shoulder is approximately 121 metres, with 1,449 passing samples across its two walking lanes and 2 m total reviewed width. Both open corridors recycle agents at their exits; vehicle exit distance accounts for the wheelbase and footprint. Runtime support and all four vehicle wheel contacts remain authoritative. Missing tiles, steep surfaces and unavailable contact stop ground movement. Validation is local to the current imagery and tile configuration.

On sloped ground, the actor's height above the surface must not accumulate as lane displacement. Lane offsets now retain the intended corridor offset, while body heading follows the ground-projected route tangent instead of terrain-refinement corrections. The `SlopedCorridor` automation test covers sustained movement on a tilted surface and retiring at an open exit. The jeep fits its chassis to axle contacts and uses a chassis-shaped obstacle sweep, with wheel contact enforced separately. Close overhead views retain tile detail around reviewed ground routes in addition to the wider air-coverage camera.

Ground spawn spacing reserves the following gap, including the wheelbase for vehicles, with a full ground-placement check before accepting the actor into the population. This prevents a new pedestrian from spawning beside a queue and blocking its own first lane change. Initial heading follows the surface-projected route tangent. Pedestrians may try nearby supported positions inside the reviewed width when a small terrain seam blocks their intended lane. They still stop if no tested position is supported. The actor's `BlockedReason` property exposes corridor, contact and collision failures for inspection. Ground streaming views are bounded to eight nearby reviewed routes; larger networks require a separate streaming-budget review.

`Living Main PTZ` is placed beside the road, with WebRTC ID `ptz-1` and the existing camera-only ROS bridge at `ws://192.168.18.9:9090/runesim`. Saved PTZ placements are now separated by map under `Saved/LivingWorld/PTZ/<Map>/<CameraId>.json`; a demo placement cannot relocate the MainLevel camera. Old unscoped placement files are not automatically migrated.

Build both scenes with `Scripts/package_living_demo.ps1`. `Scripts/run_living_demo.ps1` now opens MainLevel by default; use `-Scene LivingWorldDemo` for the flat fixture. When the user's AirSim settings have neither a simulation mode nor vehicle definitions, the launcher supplies `Samples/LivingWorld/MainLevel.settings.json`, containing one simulated SimpleFlight quadrotor. It does not overwrite the user's settings or configure physical hardware.

Open **Environment · Living World** with its viewport button or **F9**. The earlier F10 binding conflicted with AirSim's weather menu in MainLevel. F10 remains AirSim's shortcut; Living World uses F9 in both scenes.

Editor verification must use Play, not Simulate: this AirSim HUD expects a possessed vehicle. `LevelEditorSubsystem.editor_request_begin_play()` starts the appropriate mode. `Scripts/test_main_population.py` exercises MainLevel and writes `Saved/LivingWorld/main-population.json`; `inspect_main_runtime.py` reports the current categories and blocked agents. Tests in `LivingWorldDemo` do not count as MainLevel acceptance.

`LivingAgent.PreviewRoutePlacement` lets route-authoring tools run the same full placement checks as live ground agents. `hold_main_route_tiles.py`, `scan_main_vehicle_placement.py` and `apply_main_vehicle_placement.py` were used for this corridor. The apply script requires its captured candidate report; it is not a general GIS importer. Evidence is in `Saved/LivingWorld/MainRoutes/applied-vehicle-placement.json`.

The Development Editor build and all ten Unreal automation suites passed, with no suite warnings or failures (`MainFinalAutomation/index.json`). They cover geographic origin changes, missing/restored collision, sloped-corridor movement, chassis obstacles and open exits.

The final 80-second live MainLevel test passed all ten checks (`main-population.json`). It observed all twelve profiles, movement in all four air categories and all three ground categories, no sampled aircraft/terrain penetration, advancing connected camera frames, and continued ground movement after the main camera was confirmed looking skyward. The configuration requests four planes, four helicopters, ten drones, forty birds, eight people and three jeeps; vehicle recycling and safe-placement checks can temporarily reduce the active count. The sampled stream counter reached 1,668 frames. Missing collision can still hold individual birds while tiles load; this test is not a claim of uninterrupted movement for every actor.

## Packaged camera profiling

The cadence update adds [stationary-camera selection cadence](cesium-view-cadence.md), preserving camera appearance and terrain quality, and passed a ten-minute Pi check at 29.38 fps median with no freezes or restarts. The subsequent [crowd-flow build](living-world-movement.md) retains this optimization and passes its own five-minute Pi and dense-crowd checks. The following profiling and three-minute measurements describe preceding builds.

The first MainLevel deployment stayed connected for 180 seconds and passed ROS2 control/restoration, but failed strict video acceptance: 26.35 presented fps median, five new WebRTC freezes during extreme camera turns, and 20 presentation-frame drops. There were no stale samples, browser restarts, counter resets or simulator process interruptions. These results are retained in `MainPiTest/main-deploy.json` and `main-deploy-host.json`.

Unreal Insights identified Cesium tile selection as the principal CPU cost: `Cesium::updateView` had a 27.2 ms median and a 589 ms maximum in the camera-turn trace. A 30-degree terrain-detail trial avoided new freezes in its 60-second interval, but its 24.55 fps median missed the 25 fps acceptance floor. It was an experiment, not a passing deployment result.

Sensor streams now limit Cesium's requested angular terrain detail to a configurable 30-degree-equivalent budget. For narrower views, only the selection viewport resolution decreases; the real frustum, camera FOV, 1280×720 video and rendered aircraft models retain their original settings. Nearby route cameras independently maintain ground-collision detail. `TerrainDetailFovFloorDegrees` can be adjusted on the stream component. Persistent capture state also retains exposure and temporal history between scheduled frames. MainLevel's original tileset quality/cache settings are preserved; temporary cache and screen-space-error experiments were not saved.

## Deployed MainLevel acceptance — September 29

The final Development package successfully cooked both maps (`package-main-final.log`) and is deployed at `Saved/LivingWorld/Packaged/Windows`. The previous flat package remains in `PackagedBeforeMain`, and the first MainLevel package remains in `PackagedMainBeforeStreamBudget`. All ten automation suites passed again after the camera and shortcut changes, with zero suite warnings/failures (`MainStreamAutomation/index.json`). Existing unrelated telemetry initialization and AirSim asset-version warnings remain in broader build/startup logs.

The final 180-second Pi measurement passed its 25 fps median floor with **28.53 presented fps median / 14.17 minimum**, zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes, and 30 dropped presentation frames. The concurrent Windows measurement completed without a simulator process interruption. ROS2 pan/tilt/zoom, reverse motion, and clamping to 170° pan / −80° tilt / 5° FOV passed during the run; the original pose was restored. All 54 received state messages had monotonic frame and capture-time counters. Evidence is under `Saved/LivingWorld/MainFinalPiTest`, with host continuity in `main-final-deploy-host.json`.

Native UI verification confirmed F9 opens only Living World and F10 opens only AirSim's existing weather panel. The final packaged menu reported twelve profiles and two validated routes. The Pi screenshot shows the actual MainLevel terrain and an animated ground character. The simulator, independent signalling process and Pi viewer were left running.

This establishes local MainLevel functional acceptance and a short single-camera stream pass. It does not establish constant 30 fps, overnight encoder stability, multi-camera endurance, full-city navigation or uniformly detailed ground imagery. The route authoring is deliberately bounded to the supported corridor; physical flight/tire models and the other production gaps remain listed in [implementation status](living-world-implementation.md#production-gaps-and-external-dependencies).

## Extended reviewed corridors — September 30

`Scripts/extend_main_corridors.py` rebuilds MainLevel's ground network from the loaded Cesium collision surface. It runs in two stages: `hold` adds refinement cameras along the authored road, then `scan` runs after the tiles load.

| Corridor | Before | Now | Evidence |
|---|---|---|---|
| Vehicles: `Living Main Dirt road (scanned full length)` | 32 m | **312 m** | 1,220 of 1,220 full jeep placements (footprint, chassis fit, all wheel contacts) every 25 cm |
| Pedestrians: `Living Main Pedestrian shoulder (scanned full length)` | 121 m | **239 m** | 944 of 944 civilian placements every 25 cm |

How the corridors were built:

- **Road.** The road follows a smooth curve through the original authored control points, re-projected every 2.5 m onto the collision surface. The first, polyline pass failed only where the wheelbase cut the corners.
- **Wheel bridging.** The road allows **one wheel to bridge an isolated unsupported photogrammetry facet** (`MaxBridgedWheels = 1`). The chassis then rests on the plane of the other three wheels. Water- and no-walk-tagged surfaces still block, and so do two unsupported wheels. Earlier scans lost the road's full length to exactly such single facets.
- **Shoulder.** The shoulder applies the reviewed shoulder's measured side offset (3 m) along the whole road.
- **Rollback.** The superseded 32 m and 121 m corridors stay in the map, unvalidated.
- **Streaming views.** Ground refinement cameras now cover long corridors with one close view per ~120 m segment (at most 8 views, 1024-pixel selection viewports).

This validates physical support and clearance along the imaged road. It does not establish legal access, lane semantics, or water/building semantics beyond what the collision and tags express. OSM contains only two elements around this rural site, so there was no mapped network to import.

## Trail network and rebuilt shoulder — September 30 (afternoon)

The pedestrian network in MainLevel is now **643 m**: a 311 m shoulder plus three trails. Every segment passed the runtime civilian placement probe every 25 cm.

- **Shoulder** (`rebuild_main_shoulder.py`): rebuilt as an exact 3.0 m offset from the validated, smoothed 312 m road. The previous shoulder followed the original polyline and came as close as 1.65 m on bends. All 1,235 probes passed. The earlier shoulders remain in the map, unvalidated.
- **Trails** (`plan_main_trails.py`): a terrain-grid scan of 300×300 m at 3 m spacing found 4.8 ha of connected walkable ground, where the surface is within 16° of level and no step to a neighbouring cell exceeds 70 cm, which excludes canopy, walls and gullies. A* planned loop trails that leave the shoulder, cross the fields and rejoin it:
  - a 113 m south loop
  - a 75 m north loop
  - a 144 m loop that crosses the road twice
- **Route links** (`LivingWorld::LinkRoutes`): agents reaching a route end that meets another reviewed route of the same mode continue onto it rather than retiring or turning back. The trails, shoulder and crossings therefore behave as one network.

**Live check** (`test_main_network.py`, PIE with 84 ground agents): all checks passed.
- 3 crossing zones active.
- All trails in use.
- 61 transfers between routes.
- At least 75% of pedestrians moving in every sample (deliberate patrol pauses excluded).
- No pedestrian within 1.5 m of the road outside crossings.

Evidence: `Saved/LivingWorld/main-network.json`, `MainRoutes/trails.json` and `MainRoutes/shoulder-rebuild.json`.

**PIE alongside the running demo.** The editor needs `-settings=Samples/LivingWorld/MainLevel.editor.settings.json`, which uses AirSim API port 41452. Otherwise AirSim's RPC server collides with the packaged simulator's and the editor crashes at PIE start.
