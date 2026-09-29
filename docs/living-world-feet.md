# Human terrain adaptation

Civilian and soldier locomotion now includes two-bone leg IK on MainLevel's reviewed Cesium pedestrian corridor. The existing idle/walk/run Blend Space still supplies the gait. No collision actor, route permission, vehicle control or camera setting is changed by IK.

`LivingHumanAnimation` resolves the imported left/right thigh–calf–foot chains by their verified bone suffixes. It copies the unmodified animation's foot positions back to the game thread, samples reviewed ground there, and passes value-only support data to animation evaluation. The evaluator performs no world queries. Using the unmodified pose prevents last frame's correction from feeding back into the next probe.

Ground offsets are smoothed and limited to ±18 cm. Correction fades as the authored foot lifts, preserving swing clearance. Negative contacts can lower the pelvis by at most 12 cm; the solver does not stretch either leg. Toe descendants retain their local animation. The height solver does not rotate the foot; the subsequent stance layer below adds a bounded world-space anchor.

Only LOD0 and LOD1 query terrain. Each foot uses one terrain ray and retains a 5 cm margin inside the reviewed corridor; movement independently retains its full body-footprint checks. Further LODs use the authored animation. Water, NoWalk surfaces, missing collision, withdrawn routes, unreviewed corridor width and excessive height differences remove support. Pool reuse resets the offsets. Large movement, turns over 45° between updates or animation gaps over 0.2 s discard the previous contact.

Recordings include optional `foot_supports` with bone names, bounded heights and `stance_offset_cm`. Replay interpolates these recorded corrections without sampling live terrain. Older recordings remain supported, with zero planar stance correction when that field is absent. This is visual pose playback, not deterministic simulation replay.

## Bounded stance locking

The animation proxy now captures a world-space foot anchor during the low part of the authored gait. It derives contact from the unmodified foot height, with separate acquisition/release thresholds. During stance, the current mesh transform converts that anchor back into a planar foot correction. The same two-bone IK applies it without moving the collision actor or stretching the leg.

The correction is capped at 18 cm. It fades as reach grows from 18 to 30 cm, the body turns from 10 to 25 degrees, or the plant approaches its 0.7-second maximum lifetime. Lift rearms the next plant; an expired plant cannot repeatedly reacquire during the same stance. Pool reuse, missing/water terrain, distant LOD, a long frame, teleport or abrupt rotation clear the anchor. Existing one-ray-per-foot terrain probes follow the previous supported foot position and retain route checks.

This is bounded visual stance stabilization. It infers contact from foot height; authored contact curves, heel/toe roll, ground-normal foot alignment and dedicated sidestep clips remain future animation work. On sloped bodies the anchor correction is constrained to the mesh ground plane, while the existing terrain-height solver controls vertical placement.

The final `StanceFinalAutomation/index.json` reports all 12 suites passing without warnings. Tests evaluate both imported human rigs, translate their bodies while checking that the actual foot stays planted, exercise swing/turn/reach/time release, and verify replay correction and legacy recording compatibility.

The 120-second MainLevel check passed all eight checks (`main-human-stance.json`), including both rigs using stance correction, continued dense-crowd movement and advancing camera frames. Maximum sampled stance correction was 17.993 cm, within the 18 cm bound; maximum height adjustment was 8.095 cm. Both human types were inspected in a close-up soldier-follow view, then the original view and PTZ pose were restored (`stance-soldier-review.jpg`). The live replay test passed all eleven checks over 32 recorded frames, including exact interpolation of saved stance corrections at a seek point (`human-stance-replay.json`).

## Verification — September 29

- All **12 Unreal automation suites passed**, including `HumanFeet` and existing crowd, route, vehicle, replay and camera-cadence tests. `HumanFeet` evaluates both real imported skeletal meshes, checks actual raised/lowered foot bones, unchanged upper/lower leg lengths, pose restoration, water/missing support, route withdrawal and pooled resets. The restoration check compares the rendered foot with the unmodified pose from the same evaluation, avoiding a timing-sensitive comparison across evaluations. Reports: `Saved/LivingWorld/FeetAutomation/index.json` and the final `FeetBudgetAutomation/index.json`, which also checks the corridor margin.
- The **120-second MainLevel acceptance passed all six checks**: dense population, both rigs using IK, both adapting to actual terrain, finite/bounded offsets, continued movement and advancing camera frames. It observed corrections up to **7.50 cm**; the final sample had **60 pedestrians, 59 moving**. Report: `Saved/LivingWorld/main-human-feet.json`.
- The live two-rig replay test passed **eight checks** across 31 recorded frames, including restored support data, unchanged live subjects and collision-free replay copies. Report: `Saved/LivingWorld/human-foot-replay.json`.
- Close-up civilian and soldier gaits were inspected in MainLevel; temporary camera placement and view were restored afterward. Soldier review: `Saved/LivingWorld/feet-soldier-review.jpg`.

To repeat: run `Scripts/test_main_human_feet.py` through `Scripts/ue_remote.py` while MainLevel is in Play mode. It restores population settings after two minutes and now writes `main-human-stance.json`. Run `Scripts/test_human_foot_replay.py` separately; it writes `human-stance-replay.json`, refuses to interrupt an existing recording/replay and leaves its newly saved recording available as the last recording in that PIE session. These report names preserve the original height-only test evidence.

## Packaged performance and playback

The initial packaged five-minute playback check had no stalls, freezes or restarts, but its **24.94 fps median failed the 25 fps floor**. This result is retained under `Saved/LivingWorld/FeetPiTest/initial`. Crowd motion passed with 57–61 pedestrians and at least 98.2% moving in each post-warmup sample. Profiling measured 3.56 ms of human animation game-thread work per game frame (`feet-profile-before.json`). The foot probes were consequently reduced from three rays per foot to one, while preserving the explicit corridor margin and the independent body-footprint checks. `FeetBudgetAutomation` verifies the revised implementation.

The revised package passed the **300-second Pi test at 25.35 fps median / 10.19 minimum**, above the unchanged 25 fps median floor. There were zero stale/stalled samples, browser restarts, counter resets or new WebRTC freezes, and 14 dropped presentation frames. ROS2 pan/tilt/zoom and limit clamping passed with 49 monotonic state messages; the initial camera pose was restored. Evidence and a fresh screenshot are in `Saved/LivingWorld/FeetPiTest/final`.

The post-change packaged trace measured **1.97 ms per game frame** in human animation game-thread work, down from 3.56 ms (**44.8% lower**) in the preceding trace. Ground queries accounted for 1.81 ms of that remaining cost. The comparison uses the same MainLevel, saved population and camera configuration, with 2,303 and 2,405 measured game frames respectively. Reports: `feet-profile-before.json`, `feet-profile-after.json` and their source CSV/Unreal Insights traces. These captures occurred outside the playback acceptance intervals.

The simultaneous five-minute packaged crowd check passed: 57–61 pedestrians, with at least 98.2% moving in every post-warmup snapshot (`feet-budget-packaged-flow.json`). Windows monitoring passed for 320.36 seconds with no process interruption. Private memory increased by 12.68 MiB (`feet-budget-host.json`); this bounded check does not establish long-term memory stability.

`package-feet-budget.log` completed successfully, and the promoted executable's SHA-256 matched the local game build. `PackagedBeforeFeet` retains the prior crowd build; `PackagedBeforeFeetBudget` retains the initial IK build. MainLevel and the Pi viewer remain running. Camera appearance, saved population preferences and the existing Cesium cadence optimization are preserved.

## Packaged stance verification — September 29

The stance-locking MainLevel package passed the completed 300.01-second Pi playback check at **28.55 fps median / 27.35 minimum**, against the unchanged 25 fps median floor. It recorded 0 stale/stalled samples, 0 new WebRTC freezes, 0 viewer restarts, 0 counter resets and 49 dropped presentation frames.

ROS2 controls, clamps and initial-pose restoration passed with 48 monotonic state messages. The concurrent crowd test observed 56–61 pedestrians, with at least 96.55% moving in every post-warmup snapshot. Windows monitoring passed for 320.19 seconds with 0 process interruptions; private memory changed by -71.48 MiB. This bounded check does not establish overnight endurance or steady 30 fps.

Evidence: `Saved/LivingWorld/StancePiTest/final`, `stance-packaged-flow.json` and `stance-host.json`. `package-stance.log` completed successfully. The promoted executable matches the local game build (SHA-256 `5D3A5AEA001E2659E370DC4F4AB683862D5752407024514C61FA808C2A3311CD`). `PackagedBeforeStance` preserves the preceding terrain-IK package. MainLevel and the Pi viewer remain running.

## Remaining limits

This improves small height differences and adds bounded stance locking. It does not implement heel/toe rolling, foot orientation alignment, stair planning, dedicated sidestep clips or replacement ground geometry. Photogrammetry resolution still limits close-up terrain detail. Body orientation continues to follow the existing route surface normal; contact-phase authoring remains separate work.

The playback median has limited margin above 25 fps. These measurements do not claim a steady 30 fps, overnight endurance or multi-camera performance.
