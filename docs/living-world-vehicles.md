# Vehicle steering and contact suspension

The utility jeep retains its four-wheel Blender rig, source mesh, materials and distance-matched driving clip. `ULivingVehicleAnimation` adds steering and suspension through a single-node animation proxy; the worker-thread evaluator receives copied pose inputs and performs no world queries.

The front wheels use measured path curvature and their axle positions to approximate Ackermann steering: the inside wheel turns farther. Steering is limited to 35 degrees and slews at 100 degrees/second. Rear wheels retain their authored spin without steering. This is a visual addition to spline traffic, not a replacement with tire, drivetrain or Chaos Vehicle physics.

Each wheel has a ground contact trace at its reference pivot and a representative 50.94 cm radius. Suspension travel is bounded to 20 cm in either direction. Raised contact follows immediately to avoid tire penetration; extension smooths toward lower contact. All four contacts must be supported by acceptable static geometry, remain inside an explicitly reviewed route width, and avoid `Water` and `LivingWorld.NoWalk` actors. Missing or withdrawn support stops the agent. The reviewed road-strip check includes a 15 cm half-width allowance per tire. These authored dimensions are approximations for this generic generated vehicle.

On the four-wheel jeep, the chassis fits the supported axle/contact positions instead of following one terrain triangle's normal. The final per-wheel queries still enforce the original suspension limit, reviewed width, slope and surface tags. There is no sprung-mass body roll, tire force model, differential wheel-speed model or junction right-of-way logic. Ground tagging and reviewed corridor bounds remain scenario-author responsibilities.

Replay records per-wheel bone names, component-space offsets and steering angles. Inert replay components reconstruct and interpolate these values; they do not trace the live scene or control live vehicles. Wheel data is bounded to 16 entries, offsets to 100 cm and steering to 60 degrees when loading; malformed values and duplicate bones reject the load while preserving the current replay. Older recordings retain their original clip-only behavior.

Preparation: run `Scripts/setup_vehicle_articulation.py` through the editor remote helper with PIE stopped. A new `import_jeep.py` import also assigns these settings. The profile is `/Game/LivingWorld/Profiles/DA_Jeep`.

Verification includes the `VehicleSteering` and `VehicleContacts` Unreal automation suites. The contact suite loads the actual imported jeep, evaluates its wheel bones, tests an 8 cm raised patch and water/missing support, resumes movement after restoration, and checks pool reset. Run `Scripts/test_vehicle_articulation.py` for moving-agent verification in `LivingWorldDemo`; `test_scenario_replay.py` also checks saved/replayed wheel data.

## September 29 verification and deployment

Both Development Editor and game builds succeeded. All eight Unreal automation suites passed in a fresh headless process (zero warnings/failures). The neutral and 30-degree steering preview was inspected in Unreal, then removed; its screenshot is `Saved/LivingWorld/vehicle-wheel-preview.png`.

Live vehicle integration passed all eight checks across three moving jeeps and 717 sampled observations, with up to 6.09 degrees of front steering on the demo circuit. Replay passed all twelve checks, including saved wheel poses and the custom replay animation instance. The eight mixed-air and five human-locomotion regression checks also passed: 69 agents, all 58 airborne agents moving, and walking restored after route withdrawal/reinstatement.

The Development build was cooked, archived and deployed. The prior package is retained in `Saved/LivingWorld/PackagedBeforeVehicle`. The deployed executable matches the built executable's SHA-256, and F10 shows 69 active agents, 12 profiles and two reviewed demo routes. Evidence: `VehicleAutomationFinal/index.json`, `vehicle-integration.json`, `replay-integration.json`, `air-population.json`, `human-locomotion.json`, `package-vehicle.log`, and `vehicle-packaged-menu.png`, all under `Saved/LivingWorld`.

The simulator and Pi viewer are running with the CUDA-interoperability encoder workaround described in [streaming verification](pi-stream-test.md). The longer endurance result remains separate from the completed vehicle checks.

## Cesium MainLevel follow-up

Vehicles now sweep a chassis-shaped obstacle box while the wheels provide independent ground contacts. Spawn spacing includes both wheelbases and a following gap, preventing overlapping chassis from creating a permanent queue. Open-route exits retire the vehicle before its front footprint leaves the reviewed road. The contact automation now also verifies that a raised road barrier stops the chassis.

MainLevel's final 32 m corridor passed 2,599 complete jeep placements at 1 cm spacing. Unsupported terrain facets outside that continuous section remain excluded. All ten MainLevel live checks passed, including vehicle movement with the main camera turned away. This establishes bounded traffic on that authored corridor, not a general terrain-driving solver. See [MainLevel evidence](living-world-cesium.md).
