# Standalone Living World demo

The updated Windows Development build, cook, staging and archive passed on September 29, 2026 with Unreal 5.8.2. The earlier packaged menu test reached the 120-agent Busy cap and recorded/replayed all 120 subjects. The latest movement/crow build was verified at **69 agents, 12 profiles and two validated demo routes**: four planes, four helicopters, ten drones, forty birds, eight people and three jeeps. Aircraft, rotor/propeller rigs, weighted bird wings, blended humanoid locomotion, animated jeep, effects and audio are cooked into the build.

Prepare Epic's bundled signalling infrastructure once, then launch:

```powershell
./Scripts/setup_signalling.ps1  # First setup only; already completed on this machine.
./Scripts/run_living_demo.ps1
```

Rebuild it after code/content changes, with the editor and packaged demo closed:

```powershell
./Scripts/package_living_demo.ps1
```

To retain a running baseline during a build, use `-PackageRoot <separate-candidate-directory>`. The script refuses to overwrite an archive whose simulator is running. Close the demo and wait for its process to exit before promoting a candidate or copying saved preferences; retain the previous archive for rollback.

The archive is `Saved/LivingWorld/Packaged/Windows`; it is ignored by Git. The launcher now opens `/Game/MainLevel`, the project's Cesium scene. Pass `-Scene LivingWorldDemo` to open the flat fixture. Both maps are cooked. Open **Environment > Living World** or press F9. The packaged application's saved preferences are independent of the editor test session. See [MainLevel integration](living-world-cesium.md).

To reproduce the air showcase, close RuneSim and wait for its process to exit, then run `python Scripts/configure_packaged_air_demo.py` before the launcher. The script refuses to change preferences while RuneSim is running; otherwise its shutdown could overwrite the new settings. Graphics preferences are preserved.

## Independent streaming service

Setup copies the engine's infrastructure into ignored `Saved/LivingWorld/Signalling`, downloads the matching official Node 22.14.0 archive, verifies its official SHA-256 checksum, installs locked dependencies, and builds Common, Signalling and SignallingWebServer. It does not modify the engine installation or install Node globally.

The launcher starts/reuses the project-owned hidden signalling process, then opens the interactive simulator at a 30 fps cap. It refuses to replace an unrelated process on ports 80/8888. If the editor's embedded server occupies those ports, close that service/editor first. Closing the packaged demo leaves signalling available for the next launch. This is not an installed Windows service; re-run the launcher after Windows restarts.

`watch_living_demo.ps1` supervises the actual game binary in a hidden shell. A normal exit (code 0) stops supervision. Unexpected exits restart after 5/10/20/40 seconds; five failures within ten minutes stop recovery to avoid a crash loop. A named mutex prevents duplicate supervisors. Logs are in `Saved/LivingWorld/simulator-supervisor.log`. This recovers process exits, not a hung game or a broken graphics driver.

The revised supervisor passed an actual forced-exit test: PID 41452 exited with -1, and a replacement PID 34032 started 5.43 seconds later. Its window rendered the demo. Alt+F4 then produced exit 0 and stayed stopped until an explicit launch. Evidence: `Saved/LivingWorld/supervisor-verification.json`. Supervision uses `Process.WaitForExit()` so unrelated surviving descendants cannot delay detecting the game exit.

Four observed packaged-process crashes occurred in `nvEncodeAPI64.dll` (Windows event 1000, driver 591.84, exception 0xc0000409), including three overnight exits at 06:05, 08:40 and 11:00 on September 29. The supervisor recovered the exits, but the repeated fault prompted an encoder-path change.

The launcher now passes `-AVCodecs.NvEnc.D3D12UsesCUDA=true`; `[AVCodecs.NvEnc] D3D12UsesCUDA=True` is also stored in `Config/DefaultGame.ini` for new builds. In the installed UE 5.8 source, `NVENCModule.cpp` reads this from `GGameIni` and selects D3D12-to-CUDA interop instead of direct D3D12 NVENC resources. The renderer remains DX12 and video remains H.264, 720p/30 with a 6 Mbit/s ceiling. This is a tested workaround under investigation, not a confirmed NVIDIA driver fix. No driver or Windows system settings were changed. See [the latest streaming evidence](pi-stream-test.md).

The demo PTZ automatically starts `ptz-1` and connects to `ws://192.168.18.9:9090/runesim`. Player: `http://192.168.18.8/?StreamerId=ptz-1&AutoConnect=true&AutoPlayVideo=true&StartVideoMuted=true`. Local equivalent: `http://127.0.0.1/?StreamerId=ptz-1`. The existing Pi viewer reconnects automatically. The main project's real flight-controller configuration is unchanged.

This retains the existing trusted-LAN setup. No firewall, Internet exposure, authentication, subscription or credential changes were made.

## Evidence and limits

The current deployment is the MainLevel package described below. The earlier vehicle-articulation package's `package-vehicle.log` ends in `BUILD SUCCESSFUL`; `vehicle-packaged-menu.png` confirms 69 active actors, 12 profiles and two reviewed demo routes. The immediately preceding movement/crow archive is retained as `Saved/LivingWorld/PackagedBeforeVehicle`; the older `PackagedBeforeMovement` archive also remains available. See [vehicle verification](living-world-vehicles.md).

Its three-minute Pi deployment check passed at 29.93 presented fps median, with no stalls, freezes or restarts and 21 dropped presentation frames. ROS2 control/restoration passed with 72 state messages. A four-hour process/video measurement is still pending; see [current results](pi-stream-test.md).

The earlier `Saved/LivingWorld/package-movement.log` ends in `BUILD SUCCESSFUL`; `movement-packaged-menu.png` shows that build's 69-agent / 12-profile configuration. Its predecessor and 120-subject recording were preserved in `Saved/LivingWorld/PackagedBeforeMovement`. Earlier evidence remains in `package-final.log`, `packaged-busy.png` and `packaged-replay.png`. Cook/Stage previously recovered automatically from a temporary local Zen service restart.

Two simultaneous 1280x720 camera streams passed a short editor test: the Pi viewed `ptz-1`, while Chrome decoded `air-2`. Both capture counters advanced; machinery playback was verified after correcting persistent profile assignments. See [Pi test results](pi-stream-test.md) for the separate endurance measurement.

The preceding single-camera packaged measurement passed 600 seconds at 29.94 fps median (29.51 minimum), with zero stalls, additional WebRTC freezes or viewer restarts. There were 58 presentation-frame drops. ROS2 controls passed during playback. These numbers describe that bounded test, not uninterrupted operation during later editor work or deployment.

The latest movement/crow package passed a separate 600-second Pi test at **29.93 fps median / 29.15 minimum**, with zero stale/stalled samples, viewer restarts, counter resets or new WebRTC freezes. There were 84 presentation-frame drops. ROS2 controls and restoration passed with 70 monotonic state messages. Reports and screenshot use the `movement-` prefix under `Saved/LivingWorld/PiTest`. The supervised demo and Pi viewer remain running.

These preceding measurements describe flat-scene builds. Full-city Cesium performance, automatic legal road routing, physically calibrated flight, fluids and aircraft acoustics are not established by these results. Supplied aircraft redistribution rights remain unverified; this local build is not a public release package. Existing AirSim asset-version/toolchain warnings remain.

## Current MainLevel deployment — September 29

The current MainLevel package includes [terrain-aware human feet and bounded stance locking](living-world-feet.md). `package-stance.log` completed successfully; the deployed executable matched the local game build. The stance-locking MainLevel package passed the completed 300.01-second Pi playback check at **28.55 fps median / 27.35 minimum**, against the unchanged 25 fps median floor. It recorded 0 stale/stalled samples, 0 new WebRTC freezes, 0 viewer restarts, 0 counter resets and 49 dropped presentation frames. ROS2 control/restoration passed; the crowd retained at least 96.55% moving pedestrians per post-warmup snapshot. `PackagedBeforeStance` preserves the preceding terrain-IK build. The following measurements describe earlier builds.

The preceding crowd-flow build retained the corrected image, Cesium cadence optimization and saved population preferences. `package-crowd.log` completed successfully, and the promoted executable matched the local game build. `PackagedBeforeCrowd` preserves the cadence build, `PackagedBeforeCadence` preserves the camera-only build, and `PackagedBeforeCamera` preserves its predecessor. Its five-minute Pi check passed at 27.94 fps median with zero freezes/stalls/restarts and 12 dropped presentation frames. The simultaneous crowd check passed with at least 95% of pedestrians moving in each sample. See [crowd-flow evidence and limitations](living-world-movement.md). The measurements in the following paragraphs belong to preceding MainLevel builds.

`package-main-final.log` completed successfully with MainLevel and LivingWorldDemo cooked. The deployed executable's SHA-256 matched the final local game build during promotion. The former flat archive is preserved in `PackagedBeforeMain`, and the first MainLevel candidate in `PackagedMainBeforeStreamBudget`.

MainLevel's final three-minute Pi check passed at 28.53 presented fps median / 14.17 minimum, with zero freezes, stalls, browser restarts or simulator interruptions and 30 dropped presentation frames. ROS2 control/restoration passed with 54 monotonic state messages, including the real 5° zoom clamp. F9 and F10 were verified as separate Living World/weather shortcuts. The simulator and Pi viewer are running. See [MainLevel acceptance and limitations](living-world-cesium.md); this short pass does not replace the outstanding endurance work.

## Combat deployment — September 30

`Scripts/promote_living_package.ps1 -Candidate <folder> -Backup <name>` promotes a candidate archive. It keeps the previous archive, copies saved preferences forward, and enables the new Living World options (`bCombatTargets`, `SensorStreams`, `bRuntimePerches`). The current archive is SHA-256 `5FBB6A174FB974AFBF4D6DF35EFA9193C20681447FD7926F7F4372BB8DAEE9C7`.

Rollbacks are retained:

- `PackagedBeforeCombat`: the stance-locking build.
- `PackagedCombatV1`–`V3`: the intermediate combat builds described in [the streaming evidence](pi-stream-test.md).

The launcher now also passes `PixelStreaming2.UseMediaCapture 0` and `PixelStreaming2.CaptureUseFence 0`. Viewers can open `http://<host>/?StreamerId=ptz-1-seeker` or `air-1`/`air-2` next to the Pi's `ptz-1`. `Scripts/log_frame_rate.py` reports the engine frame rate from the packaged log.

`start_signalling.ps1` can report "did not become ready" when the first player page takes longer than its probe. Signalling keeps running; re-running the launcher reuses it.
