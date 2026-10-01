# Simulated engagement, shoot-down and camera feeds

Added 2026-09-30. Everything here is **virtual**: it acts only on Unreal actors that opt in through `SimTargetComponent`. Nothing is sent to the external flight controller or any physical device, and people and birds are never targets.

## Operator workflow

The MainLevel tripod (`Living Main PTZ`, stream `ptz-1`) has **Allow Simulated Engagement** enabled. Any `SimPTZ` can opt in the same way; the first opted-in tripod receives the keys below.

| Input | Action |
|---|---|
| **F7** | Designate the most central visible target (the nearest visible one if none is in view). With a target already designated, F7 cycles to the next. |
| **Shift+F7** | Cycle to the next visible target. |
| **Ctrl+F7** | Toggle tracking: the tripod slews to the target and auto-zooms so it fills about a quarter of the frame. Stopping tracking (or clearing) returns to the previous view. |
| **F8** | Launch an interceptor at the designated target. |
| **Shift+F8** | Abort (self-destruct) interceptors in flight. |

The same controls are in the Living World panel: **F9 → Simulation → Simulated engagement**, together with strike, fire and smoke screen buttons. A status line in the lower-left corner shows the designated target, range, interceptors in flight and the last result. Designation requires line of sight over static geometry (terrain and buildings), respects the tripod's pan/tilt limits and a 4 km range.

The launcher has a 2-second reload and at most two interceptors in flight per tripod.

## Views while firing

The tripod feed (`ptz-1`, and the ROS images below) has three views. Cycle them with **L1** on the PS5 controller, **F6** in the simulator, the menu button, or the ROS `engage` commands `view`, `view_tripod`, `view_missile` and `view_chase`. The chosen view is kept for later shots.

- **Tripod** (default): launching starts tracking and auto-zoom on the target, so you see the missile arrive and the burst. The camera keeps following the wreck as it falls, then returns to your previous view.
- **Missile**: the interceptor's nose (seeker) camera.
- **Chase**: a camera trailing the missile from behind, above and to the side, showing the missile, its thin grey trail and the target ahead.

After a hit or miss the view holds on the burst for 3 s. Bursts use `NS_AirBurst`, a large 0.9 s fireball with debris and smoke. The missile leaves a thin grey `NS_MissileTrail`, and all smoke uses `M_SmokeSoft`, which has radial falloff and depth fade and no square sprite edges; aircraft kills add extra bursts scaled to the target's size. The `ptz-1-seeker` feed still always shows the seeker.

## ROS images

The simulator publishes what `ptz-1` shows as JPEG `sensor_msgs/CompressedImage` on `/runesim/ptz/ptz_1/image/compressed`: 640 px wide, about 26 KB per frame. It publishes only while there is demand.

The Pi's `ptz_image_bridge` node (`start_image_bridge.sh`) supplies that demand:

- **When**: it requests images while the topic, or `/runesim/ptz/ptz_1/image_raw`, has subscribers.
- **Raw images**: it decodes `image_raw` (`bgr8`) itself.
- **Rate**: it lowers the requested rate if frames start to queue.

Measured on the Pi: 10.0 Hz compressed and 10.0 Hz raw, with the simulator unchanged at 25.8 fps. Frame timestamps come from the Windows clock, which ran about 2.5 s ahead of the NTP-synced Pi because the Windows Time service is stopped.

## Interceptor model

`ASimProjectile` is a game-style guided interceptor, not an engineering missile model:

- A 3.5 s boost ramps it from 40 m/s to 280 m/s, then it coasts with gradual drag.
- Guidance is true proportional navigation (N = 4) with a 35 g lateral limit, falling back to pursuit when the target is receding.
- A proximity fuse detonates within 7 m of the target's extent (its collision radius is added). The contact fuse also detonates on terrain or objects. Maximum flight time is 22 s.
- **Blast damage** is 150 at the burst point, 60% at the fuse radius, and fades to zero at 1.5× the fuse radius. It applies to every engageable target in range, so a ground burst beside a vehicle still damages it.
- Default virtual health: drone 35, helicopter 75, aircraft 85, vehicle 100. Profiles can override it with `TargetHealth`.
- Launch, motor and explosion sounds are original synthesized recordings (`Scripts/synthesize_living_audio.py`), with long-range attenuation and a 10-voice limit. Launch, smoke trail, detonation, impact and sparks use the existing Niagara systems.

## Shoot-down behavior

When a Living World aircraft, helicopter or drone reaches zero health, it becomes **Downed**:

- It plays an explosion and electrical sparks, its rotor audio winds down, and it keeps its momentum.
- It falls under gravity while tumbling and trails smoke.
- On terrain contact it plays a crash explosion and is recycled. Wrecks cannot be designated again.
- A disabled vehicle burns in place for 12 s, then is recycled.

The pool refills the population as usual.

## Camera feeds

All feeds use Pixel Streaming 2 on the existing signalling service.

| Stream ID | Content |
|---|---|
| `ptz-1` | Tripod sensor. While an interceptor flies, this feed switches to its **seeker view** and returns to the tripod 1.5 s after the burst (`bSeekerViewOnStream`). |
| `ptz-1-seeker` | Dedicated seeker feed: follows the newest interceptor and mirrors the tripod view when none is in flight. |
| `air-1` … `air-4` | Stabilized gimbal cameras carried by active drones (then helicopters and aircraft). The carrier changes when one is shot down or recycled, but the stream ID stays stable, so viewers stay connected. Set **Airborne sensor feeds** in the menu (default 1). |

Carried feeds (`air-N`, `ptz-1-seeker`) stream 960×540 at 15 fps; `ptz-1` stays 1280×720 at 30 fps. Secondary feeds render only while a viewer is connected (`bCaptureOnlyWhenViewed`), so unused feeds cost no GPU time. Each watched feed also requests Cesium tiles for its view; carried feeds register no Cesium view of their own (`CesiumViewScale = 0`) and see the tiles already selected for the activity area and main views: in the packaged MainLevel, two moving drone feeds with even quarter-resolution selection views cost about 17 ms per frame. Captures use the RDG copy path (`PixelStreaming2.UseMediaCapture=False`, `CaptureUseFence=False`): with MediaCapture, three or more simultaneous feeds stalled the render thread in 100 ms fence waits. The carrier's own mesh is hidden from its camera. Viewer URL pattern: `http://<host>/?StreamerId=air-1`.

## PS5 controller (Pi)

A DualSense paired with the Pi drives the tripod through ROS2 via the `ps5_ptz_teleop` node (`Scripts/pi_stream_test`, started with `start_teleop.sh`):

- **Camera**: the left stick pans and tilts, the right stick zooms, and L2 gives precision aim.
- **Engagement**: Cross designates, R1 selects the next target, Square toggles tracking, R2 launches, L1 cycles the view, Triangle aborts and Circle clears.

See the [Pi README](../Scripts/pi_stream_test/README.md#ps5-dualsense-teleoperation) for the full mapping.

## ROS2

With ROS enabled on the tripod, it also subscribes to `/runesim/ptz/<id>/engage` (`std_msgs/msg/String`, at most 32 characters). Accepted commands: `designate`, `next`, `track_on`, `track_off`, `fire`, `abort`, `clear`, `view`, `view_tripod`, `view_missile`, `view_chase`, and the battlefield effects `strike`, `smoke_screen` and `burn`, placed at the crosshair (see [battlefield effects](living-world-war-effects.md)).

The existing `/state` JSON gains an `engagement` object:

- status, target, target range, target pan/tilt
- tracking
- in flight, launches, hits, misses

The Pi's rosbridge uses a topic allow-list. `Scripts/pi_stream_test/start_rosbridge.sh` now includes the engage topic. It is deployed on the Pi (September 30), and engagement through the real ROS2 bridge passed.

## Events, log and replay

Designation, launch, hit, destruction, detonation, miss, shoot-down and crash are recorded as timestamped events:

- **Engagement log**: `Saved/LivingWorld/Combat/Engagements_<date>.jsonl`, written only in real sessions (PIE or packaged).
- **Recordings**: a Living World recording also stores these events, and it samples interceptors launched after recording started.

**Overlay last recording** replays the effects (launch smoke, bursts, shoot-downs, crashes) at their recorded times, alongside the inert pose overlay. Seeking never replays effects, and command-type events (designate, hit) are not re-issued. This reproduces what happened visually; it is not a deterministic physics re-simulation.

## Verification — September 30

**Automation.** Seven new suites were added; all 19 `RuneSim.LivingWorld` suites pass. They cover:

- Guidance against crossing, diving and receding targets.
- Area blast.
- Designation with occlusion, pan limit and range.
- Tracking, auto-zoom, cooldown, the in-flight limit, abort and the seeker switch.
- Shoot-down fall and crash, and the burning vehicle.
- Event replay and recorder events.
- Sensor camera mounting and gimbal.

**Live MainLevel (PIE).** `Scripts/test_main_combat.py`: all 13 checks passed (`Saved/LivingWorld/main-combat.json`):

- 4 launches: 3 kills (two drones, one vehicle) and one miss at 7.2 m.
- Three downed agents crashed or burned out and were recycled.
- `air-1` and `air-2` were carried by drones.
- The seeker feed was present.
- 30 birds landed on 16 runtime landing sites and took off again.
- Soldiers paused on patrol.
- The engagement log and recording contained the events, and the replay loaded 22 events.

**ROS.** A loopback rosbridge harness (`Scripts/test_ptz_engage_loopback.py`) passed subscription, designation, tracking, launch and clear through the actual `/engage` handling. Against the Pi's real bridge, `/state` including the engagement fields was received (81 monotonic messages), but `/engage` was dropped by its allow-list as described above.

## Limits

- Kinematics, blast and damage are game models; there is no aerodynamic, seeker-physics or warhead modeling.
- Designation uses ideal line of sight, with no sensor noise, clutter or IFF.
- Aircraft do not evade.
- Synthesized sounds are original and label-accurate, but not calibrated recordings.

## Engine audio

Each aircraft, drone and vehicle has its own engine pitch, varying ±8%. Ground vehicles rise in pitch with speed. Everything shifts with Doppler relative to the audio listener, clamped to 0.75–1.35×. This is still loop-based sound rather than MetaSounds.
