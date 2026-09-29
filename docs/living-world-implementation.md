# Living World implementation status

2026-09-29. This is a working simulation foundation and technical demo, **not completion of the full realism/art plan**.

Latest addition: [bounded human leg IK on Cesium terrain](living-world-feet.md), deployed in the MainLevel package. Both human rigs, dense crowd movement and recorded foot-support replay passed their checks. Bounded stance stabilization has since been added; see the same document for its release limits and validation.

## Run

Build `RuneSimEditor Win64 Development` with Unreal 5.8.2. Open `/Game/LivingWorld/Maps/LivingWorldDemo` and Play/Simulate. Open **Environment > Living World** in the viewport or press **F9**. Enable it and choose a preset, then **Apply and save**. The menu is installed by the world subsystem in existing game worlds, including MainLevel.

The population uses rigged humans, articulated jeep traffic, Mavic and Orqa drones, F-4 aircraft, Huey and Super Cobra helicopters, and pigeon/gull/crow flocks. Detailed profiles are approved for ambient use. Demo-only legacy profiles remain confined to `LivingWorldDemo`. MainLevel now has two short authored dirt-road/shoulder corridors and a streaming PTZ; see [MainLevel integration and its validation scope](living-world-cesium.md).

## Delivered

- Persistent Living World settings, Quiet/Balanced/Busy presets, civilian/military/mixed population, independent crowd/traffic/air/bird density, reactions, seed, radius, actor cap and ambient dB volume.
- Bounded actor pool, 30 Hz movement integration, eight spawn attempts per category per maintenance cycle, deterministic random seeds, profile-based meshes/animations/audio. Off deactivates only subsystem-owned agents.
- Separate walking/driving spline corridors. Ground traces consider static geometry only; missing terrain, steep slopes and tagged water/no-walk surfaces halt movement. Ground agents cannot freely leave the authored corridor. Crowds reverse along that corridor when fleeing; this is not full sidewalk/navmesh intelligence.
- Open one-way routes retire agents at their exit instead of reversing; closed one-way routes circulate. The OSM candidate converter separates mapped walking ways from roads, preserves access/direction/provenance, and excludes ambiguous structures/restrictions. Imports remain unvalidated, preserve closed loops, are undoable, and skip identical existing candidates. See [route preparation](living-world-routes.md).
- Startle/flee/recover states, line-of-sight threat checks against moving pawns, collision avoidance, flock-local cohesion/alignment/separation and bounded airborne turning.
- A bounded overhead Cesium selection camera plus camera-specific streaming views. Collision checks still fail closed if tiles are unavailable. This does not prove correctness across all bridges, tunnels or streamed cities.
- GeoJSON route importer accepting attributed WGS84 3D LineStrings. Imported routes are unvalidated by default. It does not download OSM or infer legal access automatically.
- Placeable Cesium-anchored simulated PTZ, slew/angle/FOV limits, JSON placement persistence, ROS2-compatible rosbridge command/state messages.
- Reusable Pixel Streaming 2 camera output at configurable resolution/rate. Attach `SimCameraStreamComponent` to a simulated vehicle's scene capture for a separate feed; the existing FC integration is unchanged.
- Opt-in virtual target damage, game-style projectile movement/homing, projectile camera, Niagara/sound impact hooks and bounded scenario transform/health recording.
- Eight detailed airborne profiles with four LODs each: Mavic, Orqa, F-4, Huey, Super Cobra, detailed pigeon, gull and crow. Four-propeller/dual-rotor rigs, weighted wings, flight-only F-4 gear configuration, corrected Huey blade assignments and reconstructed Super Cobra texture bindings are saved in editable Blender files. Category-specific speeds, radii, altitudes and bounded banking replace identical orbit behavior. See [air assets](living-world-air-assets.md).
- The existing Tripo utility jeep now has four articulated wheels, distance-matched wheel playback, four LODs and a textured traffic profile. It replaces the demo cube vehicles; it is a generic 4.1 m vehicle, not a verified military subtype.
- Generated pigeon, gull, crow, civilian and soldier reference images and five Tripo H3.1 models. The two humanoids have 65-bone Mixamo rigs and walk/run/idle clips, normalized to 1.70 m and 1.80 m and imported into Unreal. Reviewed Blender poses and Unreal materials; replaced the demo's human proxies. Human Blend Spaces now follow measured movement speed; blocked agents idle and fleeing blends toward running. Clips start at varied phases.
- Licensed animated pigeon from Paul Daniel Spooner/dudecon, with flapping, gliding, standing, takeoff and landing clips baked in Blender and imported into Unreal. Runtime now uses all five clips, with six flight states and one-bird perch reservations. Eight reviewed landing sites are authored only in the flat demo. Missing/withdrawn terrain, obstructions and nearby threats abort landing; departure releases the site. Its 1,420-triangle mesh is a distant-flock LOD; B-Bone approximation measured up to 2.45 cm error. The generated detailed pigeon and new gull have separate weighted flight rigs and flapping/gliding clips; they do not reuse unsuitable perching animations.
- Four human mesh LODs, with imported LOD0 preserved. Civilian vertex counts: 22,431 / 12,406 / 6,959 / 3,876; soldier: 22,335 / 12,387 / 6,979 / 3,894. Forced-LOD comparison rendered in Unreal; close-up quality and transition tuning still need scenario review.
- Editable Niagara `NS_Explosion`, `NS_SmokePuff` and `NS_ElectricalSparks` with project-authored timing, color, drift, debris and a smoke material. CPU one-shots terminate within ten seconds. Simulated targets use the explosion by default. These are conventional game effects; no fluid-based or physically calibrated explosion claim.
- Spatial helicopter, drone, jet and utility-engine loops use a shared sound class and a 12-voice concurrency limit. Helicopter recording is CC0; the other machinery loops are original sound designs. A CC0 woodland recording is available separately, not mislabelled as gull/pigeon calls.
- Menu controls record the current population, save timestamped JSONL, play inert visual overlays and clear replay. Playback supports bounded seeking, interpolation, speed, looping and sampled skeletal clip poses through `SimReplay`. Failed file loads preserve the current replay. No live actor commands are replayed.
- Official Tripo Blender and Unreal bridges passed actual pigeon transfers, including PBR textures. The generated gull also passed actual transfers to both applications. Both humanoid animation bundles transferred to Blender; editable source libraries and prepared `.blend`/FBX files are saved under `Art/LivingWorld`. Tripo spend is **315 of 1,000 authorized existing credits**, with no purchases; see the credit ledger.

## PTZ and streaming

Place `SimPTZ` in a level. Give each camera a unique `CameraId` before play. Use `SetPTZ(pan, tilt, horizontal FOV)` for local control. `SavePlacement` and `LoadPlacement` use `Saved/LivingWorld/PTZ/<Map>/<CameraId>.json` with longitude, latitude, ellipsoidal height and PTZ state. Map scoping prevents a demo placement from relocating MainLevel's tripod; legacy unscoped files are not automatically migrated.

For ROS2, run `rosbridge_server` on the ROS host, set `RosbridgeUrl`, then enable ROS on the actor. The real Pi Humble installation passed control/state tests using `ws://192.168.18.9:9090/runesim`. Use a named URL path with UE 5.8; the bare root endpoint produced a double-slash request. See [Pi setup and verification](pi-stream-test.md).

- Subscribe: `/runesim/ptz/<normalized_camera_id>/command`, `geometry_msgs/msg/Vector3`; x=pan degrees, y=tilt degrees, z=horizontal FOV degrees. ROS names replace punctuation with underscores (`ptz-1` becomes `ptz_1`) and repair empty/numeric-leading names.
- Publish: `/runesim/ptz/<normalized_camera_id>/state`, `std_msgs/msg/String`; JSON carries angles, FOV, simulation time, capture time and frame number.
- `Scripts/ros2_ptz_demo.py` is an operator example for a ROS2 environment.
- ROS has no projectile, damage or physical weapon command interface.

Start the stream through `Stream.StartStream()` or opt into auto-start. The default signalling URL is `ws://127.0.0.1:8888`. In the editor's Pixel Streaming 2 player, choose `ptz-1`. The local test player was `http://127.0.0.1/?StreamerId=ptz-1`. The demo launcher now starts independent Epic signalling from `Saved/LivingWorld/Signalling`; it survives editor/application restarts. See [standalone deployment](living-world-packaged-demo.md). Browser input is disabled on these sensor streamers.

## Verification performed

- September 29 stability repair: reloaded the Pi's Wi-Fi driver and disabled power saving; capped adaptive video at 6 Mbit/s and demo rendering at 30 fps. A clean 600-second run passed at 29.94 fps median with zero freezes/stalls/restarts. Router and Windows probes each passed 600/600 packets. ROS2 controls also passed during playback. See [stability evidence](pi-stream-test.md).
- Unreal 5.8.2 Development Editor C++ build succeeded.
- Windows Development build, cook and archive succeeded. The latest packaged menu shows 69 agents, 12 profiles and two routes. Earlier packaged Busy-cap (120 actors) and recording/replay tests also passed. See [standalone demo](living-world-packaged-demo.md).
- Six Unreal automation suites passed: settings sanitation, route integration, ground motion, flight envelope, replay validation/interpolation, and simulated PTZ/opt-in target interfaces.
- Expanded air integration: 69 active agents, including 4 planes, 4 helicopters, 10 drones and 40 birds. All 58 airborne agents moved. All seven articulated detailed air profiles passed component-space bone-motion checks, excluding actor rotation. Flocks retain one species, actor count stays bounded and bank limits hold. The small flat scene averaged about 30 fps; this is not a full-city benchmark.
- PIE report: nine checks passed, 17 active actors, all 17 moved. Verified upright orientation and bounded ground height, actor cap, on/off, player preservation and pool reuse.
- Crowd reaction test observed fleeing beside a moving pawn.
- Ten pigeon integration checks passed, covering unvalidated/missing terrain, exclusive reservation, landing height, standing clip, withdrawal-triggered takeoff, ascent, release and return to gliding. All six states were observed. The one-way integration test verified three vehicles retired at an open exit.
- Nine offline OSM policy/geometry tests passed, including access precedence, ambiguous/restricted geometry, invalid coordinates and reversed one-way geometry.
- Five editor importer checks passed: candidates remain unvalidated, direction/loop flags survive, WGS84 coordinates round-trip within 1 cm, and a repeated import creates no duplicates. The starter Niagara burst rendered and became inactive within ten simulated seconds.
- Humanoid integration test verifies advancing weighted bone poses, walk playback, blocked-route idle, and resumed walking. Spatial footstep variations use five CC0 Kenney clips and a dedicated notify track; gain follows Living World's ambient dB setting. Contact timing and surface-specific audio still need production tuning.
- After the notify persistence fix, all five humanoid checks passed again in a newly opened editor. The final population repeat passed nine checks with 17 active/moving agents.
- Loopback rosbridge protocol harness passed subscription, advertisement, state publication and command clamping. A subsequent **real ROS2 Humble test on optimuspi** passed pan/tilt/zoom, reversed motion, clamping to 170/-80/5, and restoration of the initial pose. The latest post-packaging repeat collected 108 state messages with advancing frame numbers and capture times.
- Chrome decoded live 1280x720 H.264 PTZ video at about 29–31 fps, zero observed dropped frames in a short local test. A sampled browser-reported total latency was 33 ms; this is not a measured glass-to-glass latency guarantee.
- Historical pre-repair observation (superseded by the September 29 stability result): An eight-second streamer interruption resumed successfully. The Pi watchdog also restarted its own viewer after stalls; an earlier recovered 1280x720 sample advanced at 23.57 fps, with 837 decoded and 55 dropped frames cumulatively. Intermittent LAN/SSH failures occurred during this run, so sustained 30 fps and interruption-free playback are not established.
- Historical pre-repair observation (superseded by the September 29 stability result): After packaging and restarting the editor, the Pi recovered again: a fresh 1280x720 sample advanced at 29.9 fps. Chromium reported cumulative dropped frames and GPU shared-image errors; this is not an endurance or zero-drop result. mDNS intermittently failed, so the test helper used the last verified IP while retaining the original hostname's mandatory SSH host-key check.
- Raw test reports and screenshots live under `Saved/LivingWorld` (ignored by Git). The earlier performance results in this section are for the small flat test scene. MainLevel has its own [Cesium acceptance record](living-world-cesium.md).
- Historical pre-repair observation (superseded by the September 29 stability result): A 60-second Pi software-video trial still stalled and required watchdog recovery. The viewer was restored to its default video path. Diagnostics now distinguish presented frames from decoded/received frames and reset rate calculations across browser restarts; earlier video rates used total playback frames, including dropped ones. That trial failed before the subsequent adapter recovery and successful ten-minute test.
- Editor restarts temporarily interrupt the Pi stream. Use the demo/camera/ROS helpers below to resume it. DCC connections are per Studio tab and may need reconnecting after an editor restart.

## Production gaps and external dependencies

The functional demo does not establish the entire original “highly realistic war simulator” scope. Remaining acceptance work is explicit:

1. The public park fixture passed building/water/barrier prefiltering, 55 terrain-height samples and collision retention with the main camera turned away. MainLevel's two authored corridors have local collision/width review; its mismatched OSM candidates were rejected. Wider networks, crossings, bridges, public access and changing imagery still require scenario-specific review. Ground spawning remains limited to validated corridors.
2. Authored human contact phases and heel/toe roll, detailed bird landing clips, vehicle chassis/tire dynamics, junction traffic logic and aircraft material/draw-call consolidation need further production work. The jeep now has bounded visual steering and wheel-contact suspension; see [vehicle articulation](living-world-vehicles.md). The current mechanical and bird rigs are functional visual rigs, not engineering-grade models or flight dynamics.
3. The CC-BY TB2 candidate is verified but download is blocked by Sketchfab sign-in. Epic's optional Niagara Examples Pack is blocked by Epic/Fab sign-in. No paid addon was purchased. Supplied aircraft redistribution licenses remain unverified.
4. Effects are bounded CPU game VFX. Photoreal fluid smoke/fire, branching electrical arcs, calibrated aircraft acoustics and species-specific calls are not claimed.
5. MainLevel passes local population acceptance, a ten-minute cadence-build Pi test and a separate five-minute crowd-build Pi/ROS2 test with the corrected sensor image. Multi-camera endurance, overnight encoder stability and full-city performance remain unverified. Keep the successful Pi Wi-Fi and bitrate settings. See [latest streaming evidence](pi-stream-test.md).
6. Virtual combat remains game-only. The replay is a visual pose/animation overlay, not deterministic physics/event resimulation; effects and external FC commands are not replayed.

## Reproduce development checks

- `Scripts/ue_remote.py <script>` sends a Python file to the running local RuneSim editor using Epic's bundled remote API.
- `Scripts/test_living_world_pie.py` runs the isolated demo checks asynchronously and leaves its menu open. Run `Scripts/start_demo_camera.py` in that session to start `ptz-1`. Run `Scripts/stop_demo_review.py` afterward to restore the original preferences and end PIE.
- `Scripts/test_ptz_rosbridge.py` requires `websockets==14.2`, binds loopback only, and runs for 30 seconds. The test dependency was installed in `Saved/LivingWorld/testenv`, not the system Python.
- `Scripts/prepare_living_assets.py`, then `Scripts/finish_prepared_materials.py`, run in Blender background mode. Originals remain untouched; review the draft manifest and visual output before approving profiles.
- `Scripts/import_living_assets.py` imports the representative textured drafts. `Scripts/create_living_demo.py` refuses to overwrite an existing demo map.
- `Scripts/prepare_tripo_humans.py -- Civilian` (or Soldier) runs in Blender; `render_tripo_human.py` renders review poses. `import_tripo_humans.py`, `setup_human_review.py`, `enable_human_profiles.py` and `finish_asset_review.py` run through the Unreal remote helper with PIE stopped.
- `Scripts/connect_pi_ptz.py` enables the tested ROS endpoint in the running demo. `Scripts/pi_stream_test/test_ptz_roundtrip.py` runs on the Pi and restores the starting pose after testing.
- `Scripts/prepare_footsteps.py` uses project-local `soundfile==0.13.1`; `install_footsteps.py` imports five variations and places dedicated animation notifies. Re-run the notify installer after reimporting humanoid FBXs.
- `Scripts/test_human_locomotion.py` runs after the population test finishes. Five checks cover real skeletal movement, locomotion selection, footstep playback, blocked idle and recovery. Its callback state is isolated from other remote Python scripts.
- `Scripts/test_stream_reconnect.py` deliberately interrupts only `ptz-1` for eight seconds and restarts it. The Pi viewer has a 30-second stalled-video watchdog; LAN interruptions during this test also caused intermittent SSH timeouts.
- `Scripts/setup_bird_behavior.py` assigns the five reviewed clips and eight demo perch sites, with PIE stopped. `test_bird_behavior.py` tests live transitions using temporary changes to PIE copies, then restores those sites. Production `LivingPerch` actors default to unvalidated.
- `Scripts/build_human_lods.py` generates four mesh LODs without reducing the source LOD. `review_human_lods.py` creates transient comparison placements; `finish_lod_review.py` removes them. Screenshot: `Saved/LivingWorld/human-lod-review.png`.
- `Scripts/install_footsteps.py` now calls `modify()` and forces an animation save: UE's notify-array helper does not mark the package dirty itself. A prior same-session pass missed this persistence bug. A fresh-process `verify_footstep_assets.py` check now confirms two saved notifies on each of four walk/run clips.
- `Scripts/setup_effect_preview.py` prepares the engine-native starter burst and a transient editor preview. The final editable effects are built and reviewed by `build_living_effects.py` and `review_living_effects.py`.

See [the original plan](living-world-plan.md) for the intended production scope and add-on shortlist.

## Latest reproduction additions

- `finish_aircraft_rigs.py`, `rig_generated_birds.py`, `rig_jeep.py`: editable source rigs and FBX exports.
- `import_flight_assets.py`, `enable_flight_profiles.py`, `import_jeep.py`: reviewed runtime content and LODs.
- `build_living_effects.py`, `review_living_effects.py`: Niagara authoring and render/lifetime checks. The narrow editor adapter depends on Epic’s experimental UE 5.8 authoring API; packaged runtime does not depend on NiagaraEditor.
- `prepare_ambient_audio.py` (project-local audioenv), `install_ambient_audio.py`: audio derivatives, shared class and concurrency.
- `test_air_population.py`, `test_scenario_replay.py`: population/bone-motion and menu-backed recording integration.

## Final integration checks

- Menu-backed replay integration passed all nine checks, preserving jeep/helicopter/gull meshes and skeletal poses without changing live subjects. See [replay usage](living-world-replay.md).
- Spatial machinery playback and both connected camera capture counters passed in the editor. A save-persistence bug in audio profile assignment was fixed with an explicit modification and forced save.
- The standalone launcher starts an independently built Epic signalling service. It refuses to replace unrelated port listeners and preserves the existing LAN scope. The demo PTZ and ROS connection now auto-start.
- A public Golden Gate Park OSM export produced 175 walking and 11 driving candidates after building/water/barrier-envelope filtering. These remain unvalidated. That earlier fixture did not change MainLevel; subsequent integration is documented in [MainLevel's acceptance record](living-world-cesium.md).
- A separate Cesium review map passed all 55 terrain collision samples before and after a 20-second camera turn-away. See [route evidence and limits](living-world-routes.md). The height harness's reentrant actor cleanup was corrected and a fresh-process repeat passed.
- The final 69-agent packaged demo passed a new 600-second Pi stream test: 29.94 fps median / 29.51 minimum, zero stale/stalled samples, viewer restarts, counter resets or new WebRTC freezes. There were 58 presentation-frame drops. ROS2 PTZ controls passed during the run with 70 monotonic state messages. The application and viewer remain running.

## Movement and crow update — September 29

Human idle/walk/run Blend Spaces now follow measured movement speed. Ground agents accelerate, brake for route queues and use reviewed pedestrian lanes with footprint-edge terrain checks. Only the authored demo circuits received reviewed widths and rounded corners; GIS routes remain unvalidated. Six Unreal automation suites and the human/population integration checks passed. See [movement implementation and limits](living-world-movement.md).

A new imagegen-to-Tripo crow has a weighted flight rig, flapping/gliding clips, four LODs and a representative 0.95 m wingspan. Eight mixed-air integration checks passed with all eight detailed air profiles represented and all 58 airborne agents moving. Replay passed ten checks with blended civilian and crow animation alongside jeep, Huey and gull. Tripo spending totals 315/1,000 credits, with 2,035 remaining in the account at the latest verification.

The supplied F15SMT was inspected and kept out of the active population because its visual detail did not meet the target. A free NASA Global Hawk source candidate was downloaded with provenance but is not approved or cooked into the population.

The movement/crow build was cooked, archived and deployed, preserving the earlier package in `Saved/LivingWorld/PackagedBeforeMovement`. Its F10 menu confirmed 69 actors, 12 profiles and two validated demo routes. A fresh 600-second Pi test passed at 29.93 fps median / 29.15 minimum with no stale/stalled samples, browser restarts, counter resets or new WebRTC freezes; 84 presentation frames were dropped. ROS2 control passed with 70 monotonic state messages and restored the original camera pose.

The launcher now supervises unexpected simulator exits, with bounded backoff and a crash-loop limit. A forced exit restarted after 5.43 seconds; normal Alt+F4 stayed stopped. An earlier baseline crash occurred in NVIDIA's encoder DLL, so this is recovery mitigation rather than a claimed driver fix. The updated simulator, signalling and Pi viewer are running. See [package/recovery details](living-world-packaged-demo.md) and [latest Pi evidence](pi-stream-test.md).

## Vehicle articulation and encoder investigation — September 29

The jeep now has front-wheel steering, bounded four-wheel ground-contact suspension, and recorded/replayed wheel poses. All eight Unreal automation suites, eight live vehicle checks, twelve replay checks, eight air-population checks and five human-locomotion checks passed. The updated Windows package is deployed, with the prior build retained for rollback. See [vehicle behavior, limitations and evidence](living-world-vehicles.md).

Three more overnight NVENC crashes showed that the earlier ten-minute success did not establish long-term stability. The launcher and game config now select Unreal's CUDA interop path while keeping DX12/H.264. A twenty-minute mixed-workload run had no process/viewer restart but one 0.206-second freeze, so it failed strict zero-freeze acceptance. A separate four-hour measurement was intentionally stopped after 485 seconds for MainLevel integration; its partial interval showed no freeze or restart, but it did not complete. See [current streaming evidence](pi-stream-test.md). The full production gaps listed above remain open; this is not a claim that the original realism scope is complete.

## Main Cesium scene integration — September 29

The launcher now opens MainLevel by default. Geographic flight/terrain clearance, retained ground collision, complete jeep placement validation, queue-safe spawning, map-scoped PTZ persistence and two locally reviewed corridors are integrated into the existing Cesium scene. F9 opens Living World independently of AirSim's F10 weather menu. All ten Unreal automation suites and all ten live MainLevel population checks passed.

The final MainLevel package is deployed and running. Its 180-second Pi test passed at 28.53 fps median (14.17 minimum), with zero stalls, freezes, browser restarts or simulator interruptions and 30 dropped presentation frames. Full-zoom ROS2 control and restoration passed with 54 monotonic state messages. Cesium tile-selection profiling led to a bounded sensor-terrain detail budget; actual optical zoom and video resolution remain unchanged. See [route, performance and deployment details](living-world-cesium.md).

## Corrected image and Cesium cadence — September 29

The current deployment retains the corrected camera encoding/exposure and reduces idle Cesium selection work. All eleven automation suites, ten MainLevel population checks and five live cadence checks passed. A completed ten-minute Pi run with the saved 120-actor configuration passed at 29.38 fps median / 14.58 minimum, with zero freezes, stalls, resets or restarts and 48 dropped presentation frames. ROS2 control/restoration passed, and a simultaneous 620-second host check found no simulator interruption. The application and Pi viewer remain running. See [implementation, profiling, rollback and acceptance details](cesium-view-cadence.md); the production gaps above remain open.

## Dense pedestrian flow — September 29

Blocked lane merges now try independently validated forward/sideways moves, and terrain fallbacks retain supported offsets instead of repeatedly jumping sideways. All eleven automation suites and seven dense MainLevel checks passed. A separate route-progress check confirmed all 74 eligible trips advanced at least 3 m. The deployed build's five-minute crowd check observed 57–61 pedestrians, with at least 95% moving in every sample. Concurrent Pi playback passed at 27.94 fps median with zero freezes/stalls/restarts and 12 dropped presentation frames; ROS2 control/restoration also passed. The prior build is retained in `PackagedBeforeCrowd`. See [movement behavior, evidence and remaining limits](living-world-movement.md).
