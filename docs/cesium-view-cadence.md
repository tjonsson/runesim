# Cesium view update cadence

`LivingTerrainBudgetComponent` reduces repeated tile-selection work when every observed camera is stationary. It attaches to existing Cesium tilesets when the living-world subsystem starts in the game world. The scene's screen-space error, cache, rendering, physics meshes and streamed camera appearance remain unchanged.

After all views remain still for 0.5 seconds, the tileset updates at 10 Hz. A camera position, rotation, FOV, viewport, aspect ratio or tileset-transform change restores its original every-frame cadence. Pending cooldown is reset so the moving camera need not wait for the old interval; Unreal may apply the change on the following frame. Asynchronous tile loads continue to be processed during idle updates.

The observer includes Cesium Camera Manager views, player cameras and the scene captures selected by that manager. These include the existing AirSim, PTZ and ground-route retention cameras in MainLevel. Comparison is against a retained reference pose, so small cumulative motion eventually wakes selection. Stereo rendering and unavailable/invalid view information fall back to every-frame updates.

Set the component's `bEnabled` to false to opt out. Authored nonzero actor tick intervals are respected, and the component relinquishes control if another system changes the interval. It restores its owned interval at shutdown. Tilesets created after world startup are not automatically enrolled. Offline capture or other special camera pipelines should opt out unless their view sources are included in the observer.

## Evidence

Before this change, a corrected-image MainLevel trace recorded 971 `Cesium::updateView` calls averaging 22.34 ms each, with 37.44 ms average engine tick time (`perf-current.utrace` and exported CSV under `Saved/LivingWorld`). A temporary 10 Hz probe reduced selection calls to 293 across 789 engine ticks; average engine tick time was 34.99 ms. Individual selections still cost about 22.77 ms. This reduces repeated work; it does not make each traversal faster or remove streaming hitches during camera motion. The temporary probe was restored before implementation.

The Development Editor build passed all eleven automation suites, including cumulative camera movement, settling and view-list changes (`CadenceFinalAutomation/index.json`). The 80-second actual MainLevel population test passed all ten checks, including movement in every air/ground category and continued ground movement while the main camera looked away (`cadence-main-population.json`).

`Scripts/test_cesium_view_budget.py` passed all five live checks: stationary cadence, full-rate PTZ movement, settling after motion, opt-out restoration and multiple observed views. It sampled ten views over 20 seconds and restored the original PTZ pose and enabled state (`cadence-live.json`). Run this through `Scripts/ue_remote.py` with MainLevel in Play mode, separately from other camera-control tests.

The packaged build completed successfully (`package-cadence.log`), and its executable hash matched the local game build during promotion. `PackagedBeforeCadence` retains the preceding corrected-image package. Saved population and graphics preferences were preserved. The packaged menu reported 120 active actors, twelve profiles and two validated routes; this is a different population from the 69 requested by the editor integration fixture.

The packaged trace recorded 353 selections across 991 engine ticks, averaging 22.15 ms per selection. Total selection time divided by engine-tick count fell from about 22.34 ms to 7.89 ms; this is roughly 65% less selection work per frame in these stationary-camera traces. Average engine tick time was 35.57 ms, including the 30 fps limiter and other work. The traces are short workload samples, not a guarantee for arbitrary scenes or continuous camera movement (`perf-cadence-final.utrace` and CSV).

The completed **600-second packaged Pi test passed** its 25 fps median floor: **29.38 presented fps median / 14.58 minimum**, zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes, and 48 dropped presentation frames. The current 1280×720 corrected-image stream was retained. ROS2 pan/tilt/zoom, reverse motion and the 170°/−80°/5° clamps passed during the run, followed by successful restoration of the initial pose; 57 state messages had monotonic frame and capture-time counters. Evidence and a fresh Pi screenshot are under `Saved/LivingWorld/CadencePiTest/final`.

This is a bounded single-camera acceptance result in MainLevel. It does not establish constant 30 fps, overnight encoder stability, multi-camera endurance, or traffic correctness at every crowd density. The population movement acceptance used the smaller editor fixture described above. The application and Pi viewer remain running with the saved population settings.

The concurrent host measurement also completed successfully: 620.02 seconds with the same simulator process and no interruption. Private memory increased by 61.24 MiB over that interval (`cadence-host.json`). This short measurement does not establish long-term memory stability; GPU counters in that report cover the whole device.

## Inspect a running build

The Development console can report the live component state:

```text
getall LivingTerrainBudgetComponent bUsingIdleCadence
getall LivingTerrainBudgetComponent ObservedViews
```

For a short CPU trace, use `Trace.File <absolute-output-path>.utrace cpu,frame,bookmark,gpu`, exercise the intended workload, then `Trace.Stop`. Compare selection-call count and total selection time per engine frame, not only the duration of an individual selection. Idle cadence should reduce call count while each selection retains its normal cost. A camera-turn trace should show selection returning to every-frame updates.

On the Pi, the existing playback monitor can measure an independent run with `python3 test_stream_stability.py --seconds 600 --warmup 30 --minimum-median-fps 25 --report <new-report-name>`. Start after the application and viewer reconnect, use a new report name, and require `completed: true` as well as `passed: true`. Run `Scripts/measure_stream_host.ps1` alongside it to distinguish uninterrupted playback from a recovered application restart.
