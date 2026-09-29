# Living World implementation plan

Planned: 2026-09-28. This document records the proposed work; it does not imply that the features are implemented.

Current delivery and verification are tracked in [Living World implementation status](living-world-implementation.md).

## Recommended skills

Use `game-ai` as the primary skill for crowd reactions, flocking, behavior trees, steering, and navigation. Pair it with `create-game-assets` for sourcing, generation, normalization, rigging, and asset validation, and `unreal-niagara` for visual effects. These skills are already available locally. Use `game-ui-ux`, `audio-design`, and `performance-optimization` when implementing those stages.

## Initial read-only findings

- RuneSim is configured for Unreal Engine 5.8; Unreal Engine 5.8.2 is installed.
- Cesium and Cosys-AirSim are already part of the project.
- Blender's AI connection worked with Blender 5.2.1 and the current Blender MCP add-on.
- At the initial inspection, Tripo DCC Bridge was not enabled in the running Blender instance. No Tripo module was loaded, and no transfer had been tested. Bridge setup is a separate follow-up task.
- All five inspected GLBs in the supplied model directory contained no skeletal rigs or animation clips.
- The Orqa archive contained an FBX and textures; its rig still requires inspection.
- MassCrowd, MassGameplay, and ZoneGraph are available in the installed engine but marked experimental.

Supplied assets:

- `C:\Users\tommy\Desktop\synthetic_image_gen\assets\models`
- `C:\Users\tommy\Downloads\Orqa+MRM1-5.zip`

The model folder includes DJI Mavic 3 Cine, MQ27 Dragonfire, Geranium-2, Bell UH-1, AH-1W Supercobra, F15SMT AceCombat3, and F-4 Phantom files. Validate identity, dimensions, provenance, and suitability before selecting production assets.

## Menu design

Name the feature **Living World**, under **Settings > Environment > Living World**.

| Setting | Proposed choices |
|---|---|
| Enable Living World | On / Off |
| Activity preset | Quiet / Balanced / Busy / Custom |
| Population | Civilians / Military / Mixed |
| Crowd density | Off / Sparse / Normal / Dense |
| Ground traffic | Off / Light / Normal / Heavy |
| Air traffic | Separate controls for planes, helicopters, and drones |
| Wildlife | Bird density and flock size |
| Reactive behavior | Avoidance, startle, fleeing |
| Advanced | Activity radius, population limits, scenario seed |

Turning the feature off removes ambient activity while retaining the controlled vehicle and explicitly placed scenario actors. Population density and graphics quality remain independent. Settings persist across launches.

## Implementation sequence

### 1. Establish the toolchain and test location

Confirm the existing packaged build, choose one bounded Cesium area, and record baseline performance. Set up Tripo Bridge and verify a complete transfer, including textures, scale, and animation.

Tripo currently requires a qualifying Studio plan for DCC Bridge. Its separate API integration has separate billing. See [official Tripo requirements](https://www.tripo3d.ai/integrations/blender).

### 2. Build the Living World foundation and menu

Add a RuneSim population subsystem for spawning, pooling, density changes, persistence, and seeded scenarios. Define reusable asset profiles containing dimensions, animations, movement category, sounds, and behavior. Keep this project-owned layer alongside the existing Cesium and camera subsystems.

### 3. Prepare and rig representative assets

Start with Orqa, one existing aircraft, one bird species, one civilian, one soldier, and one ground vehicle. Normalize scale using documented dimensions and check Blender-to-Unreal unit conversion.

Mechanical assets need correct pivots and articulated propellers, wheels, rotors, or control surfaces. Birds and people need deformation rigs and animation blending.

For new assets, use this production pipeline:

1. Generate consistent reference images.
2. Generate a draft 3D asset in Tripo.
3. Clean geometry, retopologize where needed, and repair materials in Blender.
4. Rig and animate the asset.
5. Export and validate inside Unreal.

Check topology, materials, collision, LODs, dimensions, and animation before an asset enters the population system. Visual geometry alone does not establish realistic flight physics.

### 4. Create navigation over Cesium

Use road, footpath, building, and water data, such as OpenStreetMap or suitable local GIS data, to build separate driving and walking networks. Validate these against terrain height and collision, including bridges, tunnels, crossings, and slopes. Missing or uncertain areas remain unavailable until validated.

Preserve source attribution and applicable data terms: [OpenStreetMap copyright and license](https://www.openstreetmap.org/copyright).

Maintain collision and navigation around active agents and cameras. Cesium's view-dependent streaming can otherwise remove terrain needed by physics when the main camera turns away. See [Cesium's explanation](https://cesium.com/learn/unreal/unreal-placing-objects-v1-x/).

### 5. Add believable movement and reactions

Use StateTree/behavior trees, perception, navigation, and local avoidance. Civilians should notice nearby moving vehicles, hesitate, move aside, flee when appropriate, and recover after the disturbance. Military characters can have distinct patrol, idle, and reaction profiles.

Birds need takeoff, flapping, gliding, landing, perching, flock cohesion, and obstacle avoidance. Aircraft need category-appropriate movement rather than identical waypoint motion.

Start with ordinary actors. Evaluate Mass for larger populations after the smaller system works. Mass supports representation and simulation LOD, but adoption should follow a performance test: [MassGameplay documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine).

### 6. Add effects and sound

Use Niagara for explosions, dust, smoke, sparks, electrical arcs, and debris effects. Use Niagara Fluids selectively for nearby effects, with cheaper baked effects at distance. Fluid simulations can be expensive: [Niagara Fluids documentation](https://dev.epicgames.com/documentation/unreal-engine/niagara-fluids-in-unreal-engine).

Use MetaSounds for varied engines, rotors, footsteps, birds, impacts, and environmental audio, with distance attenuation and occlusion: [MetaSounds reference](https://dev.epicgames.com/documentation/unreal-engine/metasounds-reference-guide-in-unreal-engine).

### 7. Add virtual PTZ and camera interfaces

Implement a placeable, georeferenced tripod with pan, tilt, zoom, mechanical limits, and saved position. Keep the existing drone FC/API integration behind its existing vehicle interface.

Start the simulated PTZ's ROS2 interface through a separate bridge process. [rosbridge_suite](https://github.com/RobotWebTools/rosbridge_suite) supports WebSocket communication; [rclUE](https://github.com/rapyuta-robotics/rclUE) currently lists Windows as unsupported.

Prototype one dedicated camera stream using Pixel Streaming 2/WebRTC, then expand to simultaneous PTZ and drone feeds. Measure latency, frame rate, timestamps, reconnect behavior, and encoder load. Include sensor views when deciding which terrain and actors stay active. See [Pixel Streaming 2](https://dev.epicgames.com/documentation/unreal-engine/pixel-streaming-2-overview-in-unreal-engine).

### 8. Add a separate virtual combat scenario layer

Introduce virtual targets, damage states, shoot-down outcomes, projectile cameras, and optional game-style homing toward designated simulated targets. Keep these actions inside Unreal, isolated from real FC commands and physical weapon control.

Add recording and replay so outcomes can be inspected consistently.

## Asset and add-on shortlist

These are candidates for evaluation, not assets already downloaded or visually validated during planning. Verify current availability, licenses, engine compatibility, and production suitability before adoption.

| Need | Candidate | Assessment |
|---|---|---|
| Turkish aircraft | [Bayraktar TB2 by TheDevilsEye](https://sketchfab.com/3d-models/baykar-bayraktar-tb2-8e5b6972f7d049f19688096e03949487) | Free, CC Attribution; listed at roughly 9.3k triangles with 4K textures |
| US ground vehicle | [Low Poly Humvee FREE](https://sketchfab.com/3d-models/low-poly-humvee-free-c0e2ddcc4ccf4d49a9303303a6307bcf) | Free, CC Attribution; inspect close-up quality and articulation |
| Ground troops | [Military Soldier, rigged](https://sketchfab.com/3d-models/free-military-soldier-rigged-e9c56308a67d4a3db62e914fafa4d198) | Free candidate; verify rig, clothing accuracy, and materials |
| Bird prototype | [Animated pigeon](https://sketchfab.com/3d-models/animated-bird-pigeon-797d27b68af3453e865149435df6aa30) | Free candidate for animation and flocking evaluation |
| Broader bird collection | [PROTOFACTOR Birds Pack](https://www.fab.com/listings/0d26d643-46e7-48eb-8b38-9a9873068d29) | Paid alternative with seven rigged, animated species |
| Human animations | [Mixamo](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html) | Free with Adobe ID; suitable for humanoid animation sourcing |
| Sounds | [Sonniss Game Audio bundle](https://gdc.sonniss.com/gdc-game-audio-bundle/) | Free sound library to audition |
| Interactive smoke/dust | [FluidNinja LIVE](https://www.fab.com/listings/80fcf53e-49f7-4635-a71c-ba81280c6618) | Optional paid evaluation after the Niagara baseline; verify UE 5.8 compatibility |
| Baseline effects | [Epic Niagara Examples Pack](https://www.fab.com/listings/0e188eca-4e54-4fb2-a9ed-d8b8a565e600) | Free; explosions, smoke, sparks, lightning and footstep examples; acquisition pending Epic sign-in |
| Footsteps | [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds) | CC0; downloaded and five concrete variations integrated |

Record provenance and license for every asset, including the supplied files. Organize military assets by country, era, and role so US, Turkish, and other NATO content remains visually consistent. Check restrictions on sending purchased assets to generative services separately from permission to use them in the simulator.

## First deliverable and acceptance

Build a small playable demonstration containing:

- The Living World menu and persistent settings.
- Correctly scaled Orqa and one aircraft.
- A bird flock with believable animation.
- Pedestrians reacting to the controlled vehicle.
- Traffic following validated routes.

Acceptance requires no building/water incursions, no terrain fall-through when cameras turn away, persistent settings, credible animation, and measured performance. PTZ streaming and virtual combat follow once that foundation passes.
