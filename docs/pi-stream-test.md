# Raspberry Pi WebRTC test

Test host: `optimus@optimuspi.local`. Workspace: `/home/optimus/Code/runesim_test`.
Every remote test command starts with `export DISPLAY=:0`.

## Overnight combat-build run — September 30, 16:00 to October 1, 00:00 Stockholm

This was an eight-hour run on the combat package with the default options: 120 agents, combat targets, two sensor feeds and runtime perches. The Pi side ran `test_stream_stability.py --seconds 28800 --report overnight-combat`; the Windows side ran `measure_stream_host.ps1 -Seconds 28800 -Report overnight-combat-host`, with `-ExecutionPolicy Bypass` because script execution is disabled on this PC.

**Continuity: passed.**

- The same simulator process (PID 45380) ran all 28,800 s, with zero host interruptions.
- There were zero stalled or stale samples, browser restarts or counter resets.

**Frame rate: failed the floor by design.** Playback was 23.95 fps median against the 25 fps floor. The engine itself runs about 23 fps with two sensor feeds; set **Airborne sensor feeds** to 0 for the earlier 26–30 fps. The 5.4 fps minimum came during the editor and packaging work below.

**WebRTC freezes: 23.**

- 3 in the first 5.7 hours, while the host was otherwise idle (about 20:08, 21:39 and 21:43).
- 20 between 23:05 and 00:00, while the editor, Cesium PIE tests, a C++ build and packaging ran on the same GPU.
- The interval is therefore a mixed workload; about 0.5 freezes per hour is the idle-host figure.

**Memory: grew in steps.** Private bytes rose from 5.8 to 7.5 GiB, about 210 MiB per hour, mostly at a few discrete points (roughly +0.3 GiB at 4.5 h and +0.4 GiB at 6 h) rather than continuously. Continuous 24-hour operation should be checked, and a Cesium tile or render-target cache is the first suspect.

**Evidence:** `logs/overnight-combat.json` on the Pi and `Saved/LivingWorld/overnight-combat-host.json` on Windows.

## Asset-batch deployment — October 1, 00:02 Stockholm

- The asset build (`RuneSim.exe` SHA-256 `BEA22BD3…3255`) was promoted; the previous archive is kept as `PackagedCombatV11`.
- The engine runs at 22.1 fps median (min 21.1) with the default options, about 1–2 fps below the previous build, since the traffic now includes the 140k-triangle tank and the new vehicles.
- The Pi's 120-second check passed: 21.96 fps median, zero stalls, restarts or freezes, and one dropped frame (`logs/assets-deploy.json`).

## Pi playback with engagement — September 30, 12:45 Stockholm

Key-based SSH to the Pi is set up. The Pi's rosbridge was restarted with the updated `start_rosbridge.sh`, which adds `engage` to its allow-list; the previous script is backed up in `logs/start_rosbridge.sh.bak-202609301227`.

**First run (failed).** It exposed a defect. Tracking an aircraft left the tripod zoomed into the sky, and the tripod's Cesium view kept refining distant horizon tiles, slowing the whole simulator to about 4 fps (4.2 fps median on the Pi). Three fixes, build `82840FFA…EC81`:

- A stream view entirely above the horizon now requests no terrain tiles.
- Stopping tracking restores the operator's previous pan/tilt/zoom.
- The proximity fuse accounts for target size. A 35 m-span Global Hawk had been missed at 22 m from its centre.

**Rerun (passed).** The 300-second Pi test (`logs/combat-deploy-2.json`) recorded:

- **Frame rate**: **26.14 fps median / 22.74 minimum**.
- **Stability**: 0 stalls, 0 freezes, 0 browser restarts; 20 dropped presentation frames.
- **Engagements**: two engagements driven through the real ROS2 bridge (`Scripts/test_ros_engagement.py`), both targets destroyed (12.2 m and 6.7 m from centre).
- **Engine**: stayed at 26 fps during tracking.

## Combat and multi-camera deployment — September 30, 01:00 Stockholm

Packaged MainLevel build `5FBB6A17…C9` (`Saved/LivingWorld/Packaged`) runs the 120-actor configuration with engagement, runtime bird landing sites and two carried `air-N` feeds. Four feeds were measured simultaneously for 138 s: the Pi on `ptz-1`, plus `air-1`, `air-2` and `ptz-1-seeker` decoded in a browser on the host.

- **Engine**: 24.0 fps (25.8 with `ptz-1` alone). `ptz-1` is paced by the engine.
- **Carried feeds**: 15.00 fps each at 960×540, with 0 dropped frames and 0 freezes.
- **Capture**: 0 PixelCapture fence timeouts.

Evidence: `Saved/LivingWorld/multicamera-packaged.json`.

**Two-hour multi-camera endurance.** The same four feeds ran from 01:02 to 03:02 on the same build and passed:

- **Process continuity**: 7,200 s with 0 process interruptions (`combat-multicam-endurance.json`).
- **Engine**: 24.6–24.7 fps in every 10-minute window.
- **Carried feeds**: exactly 15.00 fps each over 123 minutes, with 0 dropped frames.
- **Freezes**: 3 WebRTC freezes in total (`air-1` 1, seeker 2, `air-2` 0).
- **Capture**: 0 fence timeouts.
- **Memory**: private memory grew from 7.83 to 8.14 GiB.

It does not cover Pi-side playback or an overnight period; the earlier overnight NVENC crash history still needs a longer run on the CUDA-interop path.

Two defects were found and fixed on the way:

1. With Pixel Streaming's MediaCapture path, three or more encoded feeds stalled the render thread in 100 ms GPU-fence waits, down to about 3–4 fps. `Config/DefaultGame.ini` and the launcher now select the RDG copy capturer (`UseMediaCapture=False`, `CaptureUseFence=False`).
2. Moving carried-feed Cesium views cost about 8 ms each per frame. Carried feeds no longer register their own tile-selection view.

**Superseded:** Pi-side playback is measured in the section above. `optimus@optimuspi.local` accepts only password SSH, and Claude does not enter passwords. Install an SSH key to let the automated Pi checks run again:

```powershell
ssh-keygen -t ed25519 -f $HOME/.ssh/id_ed25519 -N '""'
type $HOME\.ssh\id_ed25519.pub | ssh optimus@optimuspi.local "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"
```

The Pi's rosbridge must also be restarted with the updated `start_rosbridge.sh`, which adds the `engage` topic to its allow-list. The current bridge still relays `command`/`state` and delivered 81 state messages including the new engagement fields.

## Stance-locking deployment — September 29, 23:10 Stockholm

The stance-locking MainLevel package passed the completed 300.01-second Pi playback check at **28.55 fps median / 27.35 minimum**, against the unchanged 25 fps median floor. It recorded 0 stale/stalled samples, 0 new WebRTC freezes, 0 viewer restarts, 0 counter resets and 49 dropped presentation frames.

ROS2 controls, clamps and initial-pose restoration passed with 48 monotonic state messages. The concurrent crowd test observed 56–61 pedestrians, with at least 96.55% moving in every post-warmup snapshot. Windows monitoring passed for 320.19 seconds with 0 process interruptions; private memory changed by -71.48 MiB. This bounded check does not establish overnight endurance or steady 30 fps.

Evidence: `Saved/LivingWorld/StancePiTest/final`, `stance-packaged-flow.json` and `stance-host.json`. See [foot placement and stance limits](living-world-feet.md).

## Human terrain adaptation deployment — September 29, 20:09 Stockholm

The updated MainLevel package passed a fresh **300-second** playback check at **25.35 presented fps median / 10.19 minimum**. It recorded zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes, with 14 dropped presentation frames. The unchanged median acceptance floor is 25 fps; this pass has limited margin and does not establish a steady 30 fps.

ROS2 control and clamp checks passed with 49 monotonic state messages and restored the initial pose. The concurrent crowd check observed 57–61 pedestrians with at least 98.2% moving in each post-warmup sample. Windows monitoring passed for 320.36 seconds without a simulator interruption. Reports and screenshot are under `Saved/LivingWorld/FeetPiTest/final`; the earlier 24.94 fps attempt is retained in `FeetPiTest/initial`. See [terrain adaptation and performance work](living-world-feet.md). The simulator and Pi viewer remain running.

## Dense-crowd deployment — September 29, 18:28 Stockholm

The new MainLevel crowd-flow build passed a completed **300-second** Pi test at **27.94 presented fps median / 11.19 minimum**, with zero stale/stalled samples, viewer restarts, counter resets or new WebRTC freezes, and 12 dropped presentation frames. ROS2 control, reverse movement, zoom/clamping and pose restoration passed with 56 monotonic state messages. The corrected image and Cesium cadence settings remain in use.

A concurrent packaged crowd check observed 57–61 pedestrians with at least 95% moving in every ten-second sample. Evidence: `Saved/LivingWorld/CrowdPiTest/final`, `crowd-packaged-flow.json` and `crowd-flow-host.json`. See [movement changes and verification](living-world-movement.md). The simulator and Pi viewer remain running. The preceding ten-minute result below belongs to the earlier cadence build; neither run establishes overnight stability.

## Cesium cadence deployment — September 29, 17:19 Stockholm

The corrected-image MainLevel package now passes a completed **600-second** Pi check at **29.38 presented fps median / 14.58 minimum**, above the 25 fps median floor. There were zero stale/stalled samples, browser restarts, frame-counter resets or new WebRTC freezes, and 48 dropped presentation frames. Saved settings were preserved; the packaged menu reported 120 active actors, twelve profiles and two reviewed routes.

ROS2 control, reverse motion, full zoom/clamping and initial-pose restoration passed during the run, with 57 monotonic state messages. Evidence is in `Saved/LivingWorld/CadencePiTest/final`; Windows process monitoring is in `cadence-host.json`. The simulator and Pi viewer remain running. See [selection cadence and profiling](cesium-view-cadence.md) for the change and its limits. This ten-minute result does not establish overnight or multi-camera endurance.

## Camera image correction — September 29, 16:03 Stockholm

The camera-correction package corrected the washed-out sensor image with explicit display encoding, neutral exposure and restrained post-processing. Sky/ground/horizon ROS2 checks and pose restoration passed. A completed 180-second stream run had zero freezes, stalls, restarts, resets or dropped presentation frames, but its 24.14 fps median (22.35 minimum) missed the existing 25 fps floor. This was **not** a full stream acceptance pass. See [image settings, screenshots and verification](camera-image-quality.md). The later cadence build above retains the image correction and has its own acceptance measurement.

## MainLevel deployment follow-up — September 29

The final packaged Cesium MainLevel passed a completed 180-second Pi playback test at **28.53 presented fps median / 14.17 minimum**. The configured acceptance floor was 25 fps median. There were zero stale/stalled samples, browser restarts, frame-counter resets or new WebRTC freezes, and 30 dropped presentation frames. A simultaneous host check found no simulator process interruption.

ROS2 pan, tilt, zoom, reverse motion and finite-command clamping passed during this run, including the actual 5° FOV. The initial pose was restored; all 54 state messages had monotonic frame/capture-time counters. SSH sessions set `DISPLAY=:0` first. Existing Pi Wi-Fi and viewer recovery settings were preserved.

Reports and the displayed Pi screenshot are in `Saved/LivingWorld/MainFinalPiTest`; host evidence is `Saved/LivingWorld/main-final-deploy-host.json`. The earlier failed MainLevel test remains in `MainPiTest` (five freezes), and the interrupted four-hour test remains incomplete. [MainLevel profiling and implementation](living-world-cesium.md) explains the terrain-selection budget that preceded this final pass. These are short single-camera results, not overnight or multi-camera endurance acceptance.

## Vehicle build deployment — September 29, 11:37 Stockholm

The new vehicle-articulation package passed a **180-second** 1280x720 H.264 deployment check at **29.93 fps median / 29.55 minimum**. There were zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes, and **21 dropped presentation frames**. ROS2 pan/tilt/zoom, reversal, limits and initial-pose restoration passed with 72 monotonic state messages. Evidence: `Saved/LivingWorld/PiTest/vehicle-deploy.json`, `vehicle-ptz-roundtrip.json`, `vehicle-browser-status.json` and `vehicle-stream-proof.png`.

The separate four-hour measurement started around 11:34 Stockholm was **intentionally interrupted after 485 seconds** to integrate and test the actual Cesium MainLevel. Its partial interval had no stalls, restarts, resets or freezes (median 29.94 fps, minimum 28.55 fps, 61 dropped frames). This is an incomplete run, not a four-hour pass. The Pi report is `logs/vehicle-endurance.json`; Windows samples and the interruption record are `Saved/LivingWorld/vehicle-endurance-host.json` and `vehicle-endurance-interruption.json`. No endurance test remains running from that attempt.

## NVENC CUDA interop investigation — September 29

The successful ten-minute tests below did not prevent later encoder crashes. Three further overnight application exits had the same `nvEncodeAPI64.dll` / `0xc0000409` signature. The simulator now selects Unreal's CUDA interop path for NVENC while retaining DX12 rendering and H.264 video; see [configuration and recovery](living-world-packaged-demo.md).

A **1,200-second** run on that path retained the same simulator process and viewer session, with zero stale/stalled samples or counter resets. Presented frame rate was **29.92 fps median / 26.16 minimum**. It recorded **one 0.206-second WebRTC freeze and 558 dropped presentation frames**, so the strict zero-freeze acceptance **failed**. This was a mixed-workload run: C++ compilation, headless Unreal tests and packaging also ran on the Windows host. The freeze overlapped headless testing; that correlation does not prove a cause.

The host monitor passed process continuity for 1,200.37 seconds. Private bytes grew from 4,655,161,344 to 4,688,363,520 (about 31.7 MiB); device-wide GPU counters are not attributed solely to RuneSim. ROS2 control, clamping and initial-pose restoration passed with 70 monotonic state messages. Evidence: `Saved/LivingWorld/cuda-host.json`, `overnight-encoder-crashes.json` and `PiTest/cuda-stability.json`, `cuda-ptz-roundtrip.json`. This establishes crash-free operation for twenty minutes under that workload, not a multi-hour driver fix or interruption-free playback.

`test_stream_stability.py` accepts `--report <basename>` to preserve separate runs and `--minimum-median-fps 28` to enforce a presented-frame-rate floor. `Scripts/measure_stream_host.ps1 -Seconds 1200 -Report <basename>` independently records process continuity and memory on Windows.

## Earlier movement/crow build — September 29

The new 69-agent / 12-profile packaged build passed a fresh **600-second** 1280x720 test after deployment and controlled recovery checks. Median presented frame rate was **29.93 fps**, minimum **29.15 fps**, with zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes. There were **84 dropped presentation frames**. The game remained in the same process throughout this measurement.

ROS2 pan/tilt/zoom, reverse movement, limits and initial-pose restoration passed against this package, with 70 monotonic state messages. The simulator supervisor also passed forced-exit recovery and normal-close behavior. An earlier app crash in NVIDIA's encoder DLL is documented in [the packaged demo guide](living-world-packaged-demo.md); this clean ten-minute run does not establish that the underlying driver fault is fixed.

Latest evidence: `Saved/LivingWorld/PiTest/movement-stability-report.json`, `movement-ptz-roundtrip.json`, `movement-browser-status.json` and `movement-stream-proof.png`. The simulator, independent signalling and Pi viewer remain running. The previous measurements below are retained as separate baselines.

## Expanded packaged demo acceptance — September 29

The rebuilt standalone demo passed a fresh **600-second** 1280x720 test with the verified 69-agent configuration (58 airborne agents). Median presented frame rate was **29.94 fps**, minimum **29.51 fps**. There were zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes. The browser dropped 58 presentation frames during the measurement; this is not a claim of zero dropped frames.

The independent Epic signalling process remained running across deliberate editor/app restarts. The Pi watchdog recovered after those planned interruptions, before the clean measurement began. A partial Busy-preset run was intentionally stopped when a shutdown/configuration race was found; it is not counted as a passed ten-minute run. `configure_packaged_air_demo.py` now refuses to write while RuneSim is running.

Real ROS2 control passed during the final measurement: pan/tilt/zoom, reversed motion, clamped limits and restoration of the initial pose; 70 state messages had monotonic capture times and frame numbers. A separate short simultaneous-camera test decoded `ptz-1` on the Pi and `air-2` in Windows Chrome. Only the single-camera packaged run has ten-minute endurance evidence.

The browser monitor now refreshes its screenshot every 60 seconds or on a new viewer session, so proof images follow the current scene. Evidence: `Saved/LivingWorld/PiTest/packaged-stability-report.json`, `packaged-ptz-roundtrip.json`, and `packaged-stream-proof.png`. The earlier successful recovery baseline is preserved as `stability-baseline.json`. Both the simulator and Pi viewer were left running.

See [standalone setup](living-world-packaged-demo.md) for launch and signalling details.

## September 29 stability investigation

The Pi initially lost packets even with PTZ video stopped: one router probe lost
7 of 8 replies; an HTTP probe to the Windows player timed out in 12 of 32 requests.
CPU/memory pressure and thermal throttling were not observed. Both 5 GHz and a
temporary 2.4 GHz trial failed, and reconnecting the existing Wi-Fi profile alone
did not repair the connection. No conflicting ARP reply was seen for the Pi's IP.

Applied changes:

- Disabled Wi-Fi power saving on NetworkManager profile `preconfigured`, immediately
  and persistently (`802-11-wireless.powersave=2`). The original value was `0`
  (global default); runtime power saving had been on. NetworkManager documents these
  values in its [wireless settings reference](https://www.networkmanager.dev/docs/api/latest/nm-settings-nmcli.html).
- Reloaded the onboard `brcmfmac_wcc` and `brcmfmac` modules and reconnected the existing
  profile. No forced unload, OS reboot, driver installation, VPN/firewall modification,
  or network credential change was performed. Automatic band selection was restored;
  the Pi reconnected to its original 5 GHz AP. The immediate router check then passed
  15/15 packets at 2.57 ms average RTT. This demonstrates recovery, not the underlying
  firmware defect's exact cause.
- Capped adaptive Pixel Streaming video at 6 Mbit/s in `Config/DefaultEngine.ini`.
  This suits the tested 1280x720/30 feed; higher-resolution/multi-camera deployments
  need their own bandwidth budget.
- Updated `Scripts/start_demo_camera.py` to limit demo rendering to 30 fps. The
  restarted editor also received `-ExecCmds="t.MaxFPS 30"`. An attempted console
  stop of `DefaultStreamer` did not take effect (both streamer IDs remained listed),
  so that ineffective command was removed and is not credited with the recovery.
  During recovery,
  Unreal exited and its last log entry reported a GPU capture fence timeout; the
  exact exit cause was not confirmed. The editor and simulated ROS2 PTZ connection
  were restored before starting a clean acceptance run.
- Enabled 15-second WebSocket ping/timeout settings on the camera-only rosbridge
  test service. Earlier outages had left 45 reported clients; a controlled bridge
  restart cleared them and Unreal reconnected as one client. Pan/tilt/zoom, limits
  and initial-pose restoration passed again (70 state messages) while video kept
  running. These settings retire dead connections; they do not issue camera commands.

`Scripts/pi_stream_test/test_stream_stability.py --seconds 600` measures playback
without reconnecting it. It fails on stale/stalled samples, browser/counter resets
or additional WebRTC freezes. Its report is `logs/stability-report.json`. Recovery
and deliberately interrupted runs are retained separately; they are not passing
acceptance results.

Post-recovery network acceptance passed: **600/600 replies from the router and
600/600 from the Windows host**, both with zero packet loss over ten minutes.
Average RTTs were 3.16 ms and 6.89 ms respectively. All 30 repeated HTTP requests
also passed (66.15 ms median, 83.49 ms maximum), compared with 12 timeouts in the
earlier 32-request sample. Raw reports are under `Saved/LivingWorld/PiTest`.

The clean **600-second video acceptance run passed**: 29.94 presented fps median,
29.34 fps minimum, zero stalled/stale samples, zero browser restarts/counter resets,
and zero additional WebRTC freezes. ROS2 control was exercised during the run.
The Pi viewer and camera were left running. This establishes ten-minute single-camera
stability on the tested LAN, not a guarantee against future adapter faults or a
multi-camera/endurance certification.

`recover_wifi.sh` provides the tested module reload as an explicit manual operation
if this adapter fault recurs. It interrupts SSH/video briefly and logs the result.
Do not schedule repeated driver resets as a substitute for a healthy connection.
To restore the original power-saving policy, use:

```bash
sudo nmcli connection modify preconfigured 802-11-wireless.powersave 0
sudo /usr/sbin/iw dev wlan0 set power_save on
```

The original network/streaming checks below are historical and include failures.

ROS2 Humble is the existing source installation at
`/home/optimus/Code/ros2_humble/install/setup.bash`. The test does not replace that
installation or modify the Raven environment. Its source is mirrored in
`Scripts/pi_stream_test` in this repository.

The test runs a visible Chromium window receiving the RuneSim simulated PTZ
`ptz-1` from `http://192.168.18.8/`. A ROS2 node publishes viewer diagnostics on
`/runesim_test/stream_status` (`std_msgs/msg/String`). The video transport is
WebRTC, not a ROS image topic.

Verified on the Pi: advancing 1280x720 video, approximately 29.94 decoded frames
per second in a five-second sample. The sample showed 6 browser-dropped frames
out of 1,057 decoded/display pipeline frames. This is a short LAN test, not an
endurance or zero-loss guarantee.

A subsequent ROS2 subscriber received `browser_running=true` and
`video_verified=true` with measurements 1.73 seconds old. At that point the Pi
reported 3,992 frames, 80 browser-dropped frames, and 29.75 fps in the latest
sample. The viewer and RuneSim demo were left running for review.

The test uses its own Chromium profile and a loopback-only diagnostic port 9222.
The viewer restarts its own browser after 30 seconds of fresh stalled-frame
measurements (or browser exit). It leaves other Chromium profiles untouched and
refuses to launch a duplicate if its child fails to stop. Set ROS parameter
`auto_reconnect:=false` to disable this recovery behavior. The monitor is required
for stalled-video detection; absent/stale monitor data alone does not restart it.
The optional frame monitor uses a project-local `.test_venv` with
`websocket-client==1.9.0`. No SSH password is saved in the project.

Run again on the Pi:

```bash
export DISPLAY=:0
cd /home/optimus/Code/runesim_test
./start_test.sh
```

For a background launch use `bash run_background.sh`; use `--restart` to replace
only the existing test viewer after verifying its PID command line.

The launcher sources the existing ROS2 installation and the project install, and
adds the project to `AMENT_PREFIX_PATH` because this host's colcon Python builder
did not generate that environment hook.

Keep RuneSim's demo playing and `ptz-1` streaming on Windows. The local helpers
`Scripts/test_living_world_pie.py` and `Scripts/start_demo_camera.py` start the
tested scene/feed through `Scripts/ue_remote.py`. Stopping PIE ends the feed.

Logs and measurements on the Pi are in `logs/viewer.log`, `logs/chromium.log`,
`logs/monitor.log`, and `logs/browser-status.json`. The captured Pi browser
screenshot is `logs/stream-proof.png`. Copies of the screenshot and measurements
are under the Windows project's ignored `Saved/LivingWorld/PiTest` directory.

## Real ROS2 PTZ control

The camera also passed a real Humble/rosbridge round trip on this Pi: pan/tilt/FOV
commands, reversed motion, limits (170/-80/5), and restoration of the starting
pose. The latest post-packaging repeat observed 108 state messages with monotonically increasing frame
numbers and capture times. This validates simulated camera control, not physical
actuation. Raw evidence: `logs/ptz-roundtrip.json`, copied locally under
`Saved/LivingWorld/PiTest`.

An eight-second Unreal streamer interruption resumed successfully. After later
stalls, the viewer watchdog restarted its own Chromium process and decoded video
advanced again. The latest recovered sample measured 1280x720 at 23.57 fps, with
837 decoded and 55 dropped frames cumulatively. LAN connectivity and SSH also
intermittently failed during this run; this is a recovery check, not a sustained
30 fps or endurance result.

After the standalone packaging check and a subsequent editor restart, the Pi
recovered 1280x720 video again at roughly 30 fps (short samples of 29.9–30.02 fps).
The three-command ROS2 round trip and initial-pose restoration also passed again.
Chromium still reports some dropped frames/GPU shared-image errors. Intermittent
mDNS lookup failures were worked around using the Pi's previously verified LAN
address while retaining the original hostname's mandatory SSH host-key check.

On September 29, a 60-second opt-in software-video trial still stalled and needed
watchdog recovery. The default video path was restored. GPU log errors alone do
not establish the cause. Earlier `observed_fps` measurements used total playback
frames, including dropped frames; the monitor now measures presented frames and
records separate WebRTC receive/decode counters when available. Browser restarts
reset the rate baseline instead of producing negative FPS. Sustained playback
remains an open acceptance item.

A subsequent 90-second default-path sample initially presented roughly 29–30 fps,
then stalled: WebRTC received bytes, received frames and decoded frames all stopped
increasing together (68,627,070 bytes; 1,827 received/decoded frames). Statistics
continued to refresh while Unreal's capture counter advanced. This narrows the
failure to the sender/transport path or receiver transport, rather than establishing
a display-only fault. Zero reported lost packets does not prove that the connection
remained healthy. Raw samples are in `logs/default-video-soak.json`.
SSH also timed out during this investigation. A later fetch succeeded and showed
recovered 1280x720 playback at 29.52 presented fps (1,258 presented frames and 11
dropped frames in that browser session). Both soak reports and the updated browser
diagnostics were copied into `Saved/LivingWorld/PiTest`. Recovery is working;
interruption-free streaming is not yet verified.
The Pi's route to the Windows host uses `wlan0`; Ethernet is down. A thermal
sample was 53.45 C and `vcgencmd get_throttled` returned `0x0`. Wi-Fi power-saving
state was not measured because `iw` was unavailable on the test shell's PATH.
No network settings or system packages were changed for this investigation.

`build_rosbridge.sh` builds official rosbridge_suite 2.0.8 at commit
`4734f01de7867525141121452d2db0582ea63d2d` in `bridge_ws`, using a separate
`.bridge_venv`. It leaves the existing Humble and Raven environments intact.

```bash
export DISPLAY=:0
cd /home/optimus/Code/runesim_test
./build_rosbridge.sh  # First setup only; already built on the test Pi.
./start_rosbridge.sh
```

The tested endpoint is `ws://192.168.18.9:9090/runesim`. Use the named path:
Unreal 5.8 sent a double slash for the bare root endpoint, which rosbridge rejected.
The test server binds the Pi LAN address and exposes only the camera command and
state topics; it does not start the general rosapi server. This is a trusted LAN
test configuration, without TLS or authentication, not an Internet deployment.

Camera ID `ptz-1` remains the WebRTC streamer ID. ROS topic components replace
punctuation with underscores, so this camera uses `/runesim/ptz/ptz_1/command`
and `/runesim/ptz/ptz_1/state`. Commands are `geometry_msgs/msg/Vector3` with
x=pan, y=tilt and z=horizontal FOV, all in degrees. State is JSON inside
`std_msgs/msg/String`.

Run `python Scripts/ue_remote.py Scripts/connect_pi_ptz.py` on Windows after
starting the demo and its camera. In a second Pi shell:

```bash
export DISPLAY=:0
cd /home/optimus/Code/runesim_test
source /home/optimus/Code/ros2_humble/install/setup.bash
python3 test_ptz_roundtrip.py
```

The test visibly moves the simulated camera, checks fresh frame/state messages,
and restores the initial pose even when a check fails.

## Stop the viewer test

```bash
cd /home/optimus/Code/runesim_test
kill -INT "$(cat logs/viewer-node.pid)"
kill "$(cat logs/monitor.pid)"
```
