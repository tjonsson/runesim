# Battlefield effects (NiagaraFluids)

RuneSim now has a battlefield effects layer: ground strikes, burning wrecks, smoke columns, smoke screens, impacts and scorch marks. Close fires use a NiagaraFluids volumetric simulation; everything else uses sprites that stay visible at kilometre range from the tripod and sensor cameras.

## Using it

| Where | How |
|---|---|
| PS5 controller (Pi) | **Create**: artillery strike at the crosshair. **PS**: smoke screen at the crosshair. |
| ROS `/runesim/ptz/<id>/engage` | `strike`, `smoke_screen`, `burn` (a fire), placed where the tripod crosshair meets the ground within 4 km. |
| Console (`~`) | `sim.fx strike 2`, `sim.fx fire`, `sim.fx smoke_screen`, `sim.fx impact_metal`, `impact_rock`, `impact_sand`, `impact_water`, `sim.fx stop`. The optional number is the scale. It aims with the first tripod, or the view if there is no tripod. |
| Automatic | A shot-down ground vehicle burns for 30 s. A crashing aircraft makes a ground strike and leaves a fire. Interceptor hits keep the air burst. |

The tripod commands need **Allow Simulated Engagement** on the tripod, the same as interceptor launches. Every effect is recorded as an event and replays with the scenario recorder.

## What each effect is

| Type | Visual |
|---|---|
| `strike` | Bright flash, thrown grit and a brown dust cloud, and a dark smoke column. Scale 1 or more also leaves a fire for 20 s plus 10 s per unit of scale. Scorch decals are available (`bScorchDecals`) but off by default: they barely show on Cesium photogrammetry and their cost there is unmeasured. |
| `fire` | Flipbook flames, with a smoke column rising from above the flames. The closest-looking fires add a NiagaraFluids gas fire (see below). |
| `smoke_screen` | A wide white obscurant cloud that persists for 45 s. |
| `impact_*` | Surface-specific small-arms or fragment impacts: metal sparks, rock, sand, water. |
| `crash` | A ground strike with a fire (aircraft impacts). |

## Fluid level of detail

A NiagaraFluids 3D gas simulation is expensive, so it only runs where it is worth it. Every half second the war layer looks at every camera that matters:

- the local view
- each tripod feed
- each airborne sensor feed

A fire gets the fluid simulation when it covers at least **6 % of some camera's view width**. That includes a tripod zoomed onto a fire 2 km away. At most **two** fires simulate at once, the largest-looking first. A fire that is already simulating keeps it until it drops below 60 % of the threshold, which stops flicker. All other fires burn with sprites only.

| Console variable | Default | Meaning |
|---|---|---|
| `sim.fx.MaxFluidFires` | 2 | Fluid fires at once (at most 4); 0 disables fluids. The pool is created when the world starts, so set it in config. |
| `sim.fx.FluidScreenFraction` | 0.06 | View-width fraction needed for a fluid fire. |

## Performance

Measured on the packaged MainLevel with two sensor feeds and 120 agents; the baseline is 24 fps.

| Case | Frame rate |
|---|---|
| Tripod looking down at terrain 10 m away (70° FOV), no effects | 17.2 fps |
| Same view with a fluid fire | 16.5 fps |
| Tripod at 3° down and 30° FOV with a strike and a fire, after the tripod fix below | 24.1 fps |

A fluid fire filling the tripod view therefore costs about 0.7 fps. The first two rows were measured before the tripod fix, which explains their lower baseline. Pi playback with the deployed build was 24.1 fps median with no freezes over 2 minutes.

Measures that keep it cheap:

- **No create/destroy during play.** Fluid fires come from a pool created when the world starts and parked 100 km below the scene. Fires borrow and return components; none are created or destroyed during play.
- **Prewarm.** Every war effect is spawned once at start, out of sight, so its GPU pipelines compile during load.
- **Precache.** `fx.Niagara.Emitter.ComputePSOPrecacheMode=2` precaches the simulation shaders.
- **Cached assets.** Effect assets are loaded and held when the world begins play, so no synchronous load happens during a strike.
- **No collision gathering.** The fluid template's collision query gathered every movable agent (1,800 physics boxes, over its limit). It now only considers actors tagged `RuneSimFluidObstacle`, and none are tagged.
- **Half-resolution volumes.** `r.HeterogeneousVolumes.DownsampleFactor=2`.

**Tripod slowdown (fixed).** Aiming the tripod just below the horizon used to slow the whole simulator, with or without effects. An Unreal Insights trace showed `Cesium::updateView` taking about 116 ms per frame on the game thread. The tripod feed registers its own Cesium selection view, and at feed resolution near the horizon that view made Cesium traverse tiles all the way out to the horizon.

The feed view now registers at a quarter of the feed size (`sim.camera.CesiumDetail`, default 0.25; 0 registers none). The feed looked the same at full detail and at 0.25 at every pose compared: the near-ground stall pose, the default pose, a 12° zoom on the far peak and a 14° zoom on ground a few hundred metres away. MainLevel's finest tiles are already selected at that size. The packaged MainLevel (two sensor feeds, 120 agents) measured:

| Tripod pose (pan, tilt, FOV) | Detail 1 (before) | 0.5 | 0.25 (default) | 0 |
|---|---|---|---|---|
| 30, −3, 30 | 4.3 fps | 8.2 fps | 24.4 fps | 25.5 fps |
| 30, −3, 70 | 14.8 fps | 24.5 fps | 23.8 fps | 25.2 fps |
| 30, 18, 85 (default) | 22.6 fps | 24.1 fps | 23.8 fps | 25.4 fps |

In the deployed build, the stall pose with a strike and a fire runs at 24.1 fps. Pi playback was 24.0 fps median over 2 minutes, with no freezes. Raise the value only for a dataset with finer tiles than MainLevel's, and check this pose when you do.

## Sources and tuning

The effects come from the Rook & Bolt project: its coastal explosion, smoke and flame systems and its vehicle fluid fire. They are copied unchanged into `/Game/LivingWorld/Effects/RookAndBolt`. `BP_CoastalBurst` was left out because it depends on Rook & Bolt's game instance. The fluid fire derives from Epic's bundled `NiagaraFluids` template `Grid3D_Gas_Fire`; see `CREDITS.md` in Rook & Bolt.

`Scripts/build_war_effects.py` derives the war set in `/Game/LivingWorld/Effects/War` and retunes it from Rook & Bolt's close top-down camera to kilometre tripod views:

- Camera-distance culling (85 m in Rook & Bolt) is off.
- Sprites are enlarged: flash 16–32 m, dust 5–12 m, smoke 9–20 m, and flame quads 11–26 m, because the flame fills only a narrow band of each atlas frame.
- The flash lasts 0.5–0.8 s so it shows on 10 Hz ROS images.
- Fluid smoke is thinner, so the simulation box does not show from side-on views; the sprite column carries the plume.

## Implementation

- `USimWarEffects` (`SimWarEffects.h/.cpp`) is a tickable world subsystem for game and PIE worlds.
  - It manages fires (with an optional actor attachment), smoke screens and fluid selection.
  - The pure functions `SimWarEffects::ApparentFraction` and `SelectFluid` hold the fluid rule.
- `SimEvents::PlayEffect` routes `strike`, `crash`, `fire`, `smoke_screen` and `impact_*` to it. In editor worlds, which have no war layer, the older single explosions still play.
- `ASimPTZ::AimPoint` finds the crosshair's ground point; `ExecuteEngagementCommand` accepts `strike`, `smoke_screen` and `burn`.
- The project enables the `Niagara` and `NiagaraFluids` engine plugins.
- **Tests:**
  - `RuneSim.LivingWorld.WarEffects` covers apparent size, the budget, hysteresis, following the view, expiry and screens.
  - `ShotDown` checks that a wreck burns and that its fire goes out when the vehicle is recycled.
  - `Scripts/test_main_war_effects.py` drives the tripod commands in MainLevel PIE.
  - `Scripts/test_war_effects_gallery.py` photographs each effect (`Saved/LivingWorld/Gallery-*.png`).

## Limits

- The flames are camera-facing sprites. From directly overhead they read as a narrow tongue; they look best from tripod and oblique drone angles.
- The fluid fire is Rook & Bolt's vehicle-scale simulation (8 × 8 × 12 m at scale 1), not a building- or forest-fire model.
- There is no crater geometry: the scorch is a decal.
- Effects are visual only; they do not damage agents. Interceptors remain the damage path.
