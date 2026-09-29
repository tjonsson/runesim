# Ground movement and human blending

The subsequent [human terrain adaptation](living-world-feet.md) adds bounded leg IK on reviewed Cesium ground, with recorded foot supports for replay. Bounded world-space stance stabilization now supplements the height correction.

The September 29 movement pass adds idle/walk/run Blend Spaces for the civilian and soldier. The input is actual speed divided by cruise speed, bounded to 0–2.5. Sample weights smooth at 5/s, and only the dominant sample emits footstep notifies. Reused actors restart animation after Living World is switched off and on. Existing clips and editable rigs remain available.

Ground profiles expose acceleration and braking in m/s². Agents calculate a conservative stopping speed from the free route distance to the next agent, including one integration step of reaction distance. Final collision sweeps still stop unexpected contacts. This is visual route following, not a tire/suspension dynamics model.

`LivingRoute.ReviewedHalfWidthCm` is an explicit clearance limit. Zero retains centreline-only behavior. A positive value allows supported offsets within that width; both sides of the collision footprint must find acceptable ground. Water/NoWalk tags, missing terrain, steep normals, a footprint outside the corridor, or an edge more than 30 cm from the centre surface reject the move. Pedestrians keep right in sufficiently wide corridors and can pass opposing pedestrians. This does not infer traffic laws, legal road width or access permission.

The flat demo has reviewed widths: 2 m either side of its walking circuit, 4 m either side of its driving circuit. Its corners use curved splines. `finish_demo_routes.py` checked 100 positions × three offsets on each circuit before saving. MainLevel subsequently received a reviewed 2 m pedestrian half-width and 2.5 m vehicle half-width on its local corridors; see [MainLevel route validation](living-world-cesium.md). GIS candidate routes remain unvalidated with zero lateral permission.

## Dense-crowd lane merging

Pedestrians now separate a blocked diagonal lane-change step into forward-only and sideways-only alternatives. Each alternative still requires valid ground under the footprint and a full collision sweep. A blocked merge can therefore continue along its current clear lane; a blocked forward step can still yield sideways when space exists. A wall spanning the corridor continues to stop the pedestrian. This is local steering within an authored corridor, not general navigation around arbitrary buildings.

If the preferred lane has unsuitable ground, the agent first tries to retain its currently supported offset. Any alternative offset is bounded by the existing 1 m/s lateral movement rate. The previous fixed 20 cm offset fallback could repeatedly snap between positions on sloped terrain, exaggerating measured movement without making route progress.

The `GroundMotion` regression reproduced a stationary blocked merge before the fix and passes after it. It also covers a close direction reversal, minimum pedestrian separation, staying inside reviewed bounds and stopping at a full-width obstruction. All eleven Unreal automation suites passed (`CrowdAutomation/index.json`). `Scripts/test_main_crowd_flow.py` provides a separate three-minute dense MainLevel test, including a camera-away interval, and restores the original settings and camera view.

That live test passed all seven checks. The final sample had 60 pedestrians, all moving, with no pedestrian stationary for 30 seconds. The smallest sampled separation between pedestrian collision-sphere surfaces was 11.72 cm. Both civilian and soldier profiles moved, and ground motion continued while the main camera looked skyward. Evidence: `main-crowd-flow.json` under `Saved/LivingWorld`.

An independent 60-second route-progress measurement passed: all 74 eligible route trips (at least 20 seconds observed per trip) advanced at least 3 m along the reviewed corridor. Pooled respawns were excluded from displacement measurements. Run `Scripts/measure_main_crowd_progress.py` during the dense test; evidence is `main-crowd-progress.json`. This distinguishes actual route travel from sideways animation or oscillation. The prior running build's snapshot had 60 of 61 pedestrians stationary; that snapshot is diagnostic context, not a controlled performance comparison.

To reproduce, open MainLevel in Play mode and run `python Scripts/ue_remote.py Scripts/test_main_crowd_flow.py`. Once its population has warmed up for 20 seconds, run `python Scripts/ue_remote.py Scripts/measure_main_crowd_progress.py`. Keep PIE active until both reports complete, and do not run other population or camera-control tests concurrently. These scripts write their named local reports; preserve earlier reports separately when comparing runs.

Foot locking, dedicated sidestep clips, wider route networks, intersections and general obstacle replanning remain separate work. Dense populations still respect the shared actor budget and available reviewed route space, so requested counts are not guaranteed to be active at every instant.

## Packaged MainLevel verification — September 29

`package-crowd.log` completed successfully, and the promoted executable matched the local game build. The previous cadence build remains in `Saved/LivingWorld/PackagedBeforeCrowd`. Saved population and graphics preferences were preserved, as were the corrected sensor image and Cesium cadence optimization. An initial packaging attempt encountered the still-closing editor's Live Coding lock; after the editor exited normally, packaging succeeded.

A separate **300-second packaged crowd measurement passed**. Ten-second snapshots observed 57–61 pedestrians, with at least 95% moving in every sample (`crowd-packaged-flow.json`). This samples motion and does not assert that every individual was moving at every instant. The more detailed editor test above separately checks route progress and collision clearance.

The concurrent **300-second Pi playback test passed** at **27.94 presented fps median / 11.19 minimum**, above the 25 fps median floor. It recorded zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes, and 12 dropped presentation frames. ROS2 pan/tilt/zoom, reverse motion and limit clamping passed with 56 monotonic state messages, followed by restoration of the starting pose. Reports and a fresh Pi screenshot are under `Saved/LivingWorld/CrowdPiTest/final`. This is a bounded single-camera result, not overnight or multi-camera endurance. MainLevel and the Pi viewer were left running.

Windows monitoring also passed for 320.18 seconds with no simulator process interruption. Private memory increased by 0.76 MiB over that interval (`crowd-flow-host.json`); this short check does not establish long-term memory stability.

Verification:

- Six Unreal automation suites passed in a fresh headless process. GroundMotion exercises acceleration, stationary-leader braking, release/resume, immediate route withdrawal, footprint bounds, water under an edge, and opposing pedestrians passing without overlap.
- Five human integration checks passed: weighted poses, blend input versus real motion, footsteps, blocked idle, and resumed walking. An initial failure exposed a paused animation on pooled actors; that defect was fixed before the successful rerun.
- The nine population checks passed, including switch-off, preserving the player and reuse without growing the existing pool. The test now accounts for a larger pre-existing inactive pool when a smaller actor budget is selected.
- Replay records and reapplies blend inputs. The ten-check integration passed with jeep, Huey, gull, civilian and crow subjects.

Evidence: `Saved/LivingWorld/MovementAutomation/index.json`, `human-locomotion.json`, `pie-report.json`, and `replay-integration.json`.

The subsequent [vehicle articulation update](living-world-vehicles.md) adds bounded front-wheel steering and four-wheel contact suspension to the jeep. Remaining refinements include authored foot contact phases, contact synchronization on uneven surfaces, chassis/tire dynamics, junction traffic logic and detailed bird landing clips.
