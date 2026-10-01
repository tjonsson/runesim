# Airborne asset implementation

The airborne family uses the supplied Mavic 3, Orqa, F-4, Huey and Super Cobra models, plus generated detailed pigeon, gull and crow meshes. Editable flight variants live in `Art/LivingWorld/Prepared/Flight`; original source files are preserved. Runtime packages live under `/Game/LivingWorld/Models/Flight`.

## Art and scale decisions

Natural daylight, textured materials, readable silhouettes and physically sized geometry are the target. These are visual simulation assets, not dimensionally certified aircraft models. The supplied assets have unverified redistribution rights; retain them for this private project until their original licenses are established.

- Mavic: source centimetres converted to metres. DJI's [Mavic 3 specifications](https://www.dji.com/nl/mavic-3/specs) provide the reference class; source geometry is retained rather than distorted to force every bounding dimension to match.

- Orqa: scaled from the [published 5-inch propeller diameter](https://orqafpv.com/products/mrm1-5), with four articulated props.

- Huey: source metres retained. Inspection identified and corrected reversed blade assignment in the initial rig: numbered blade segments form the main rotor; the unnumbered pair forms the tail rotor.

- Super Cobra: original USD OmniPBR texture bindings reconstructed as Blender/Unreal materials. Both rotor assemblies are articulated.

- F-4: dedicated airborne variant with the separate gear hardware stowed and doors closed; the ground source remains intact.

- Generated birds: representative spans of 0.68 m (pigeon), 1.40 m (gull) and 0.95 m (crow), with two joints per wing, weighted shoulder transitions and distinct flapping/gliding clips. These profiles are flight-only. The licensed low-poly pigeon retains the separately tested landing/perching behavior.

Rotors use sampled visual animation, not a physically accurate RPM signal. The asset rigs do not define real aircraft controls or flight dynamics.

## Generated gull provenance

Reference: `Art/LivingWorld/References/herring-gull.png`, generated with the built-in imagegen tool on September 29, 2026. Prompt: one photorealistic adult European herring gull, full symmetric spread-wing neutral gliding pose, elevated front three-quarter view, complete wing tips and tail, layered gray and black-tipped feathers, white body, yellow beak, tucked pink feet, neutral studio light and plain gray background, no text or additional objects.

Tripo H3.1 task: `fe593b72-bc1e-406a-9bd4-1f71f0c0ddac`. Generated GLB and 4K textures were downloaded and saved under `Art/LivingWorld/Generated/Gull`. Cost: 55 credits; cumulative authorized use: 260 / 1,000. No purchase or subscription change.

## Reproduction

1. Run `finish_aircraft_rigs.py` in background Blender for Mavic3, UH1, AH1W and F4.

2. Run `rig_generated_birds.py` for DetailedPigeon, Gull and Crow.

3. Render flight variants with `render_prepared_asset.py -- <name> --flight --frame <frame>`; birds additionally accept `--clip Flapping` or `--clip Gliding`.

4. With PIE stopped, run `import_flight_assets.py` through `ue_remote.py`. Profiles remain unapproved until engine inspection.

## Verified results

Eight detailed air profiles imported with four LODs apiece. Unreal renders verified textures and the corrected rigs. The mixed scene spawned 69 agents: 4 planes, 4 helicopters, 10 drones, 40 birds and 11 ground actors. All 58 airborne agents advanced; component-space sampling confirmed motion in all seven articulated detailed profiles. The F-4 uses a rigid flight mesh. Species remain consistent within each flock and bank limits hold.

Mean frame time was approximately 33â€“34 ms at the demo's 30 fps cap. This is a small flat acceptance scene, not a full Cesium performance guarantee. The Pi decoded a short expanded-scene 1280Ã—720 sample at 30 fps with zero reported packet loss/freezes.

Both Blender and Unreal DCC bridges completed actual transfers of the new gull. The source families contain many material slots on some aircraft; material consolidation remains a production optimization.

The existing generated jeep was downloaded without new credits and added separately to `Prepared/Ground`. Four wheels use distance-matched animation; mesh vertex counts across four LODs are 26,268 / 16,464 / 10,125 / 6,223.

## Crow addition

`DA_Crow` uses a new generated reference and Tripo H3.1 task `29b878a8-ebe4-45b9-8d10-a86f760e8f4f` (55 credits). The reference, original 4K GLB and provenance live in `Art/LivingWorld/Generated/Crow`. Editable rig and FBX are in `Prepared/Flight`. Wingspan is an artistic 0.95 m reference; the five deform bones provide 4 Hz flapping and gliding. Four LODs contain 22,281 / 12,464 / 6,900 / 3,827 vertices. Blender and Unreal renders were inspected, symmetric wing-tip motion checked, and actual bone animation passed the mixed-population integration. Detailed generated birds remain flight-only.

The credit ledger now totals 315 of the 1,000 authorized credits. No purchase or subscription change occurred.

## Landing, perching and calls — September 30

`Scripts/add_bird_perch_clips.py` adds **Landing** (flare, then fold, 0.8 s), **Perched** (folded wings with breathing and head-bob, a 3 s loop) and **TakeOff** (0.6 s) to the detailed pigeon, gull and crow rigs. Each species has a resting pose tuned from rendered previews: the gull's wingtips cross over the tail, and the upright pigeon mesh is pitched forward.

The generated meshes are modeled in flight posture with tucked legs, so a perched bird reads as resting or sitting at sensor-camera distances; it is not a close-up standing pose. `PerchHeightCm` (pigeon 14.7, gull 19.9, crow 14.4) seats the body on the surface. Profiles now allow perching.

With **Bird landing sites on walkways** enabled, the population places up to 16 landing sites along reviewed pedestrian corridors, alternating sides. Birds search within 150 m. They flush when a person comes within 4 m or a vehicle within 9 m, as well as on an alarm.

In the live MainLevel test, 30 birds landed, perched and took off again.

Species calls (pigeon coo, gull "kyow", crow caw; three synthesized variations each) play every 12–18 s per bird, with an alarm call on startle. They use 70 m attenuation and a 6-voice limit.

## NASA Global Hawk — September 30

The downloaded NASA model (credit NASA / Michael D. Carbajal; NASA media usage guidelines, no endorsement implied) is complete. The "detached wing" in the earlier review was an artifact of the render setup, not of the geometry.

- `Scripts/prepare_global_hawk.py` scales it to the RQ-4A's published 35.4 m span (NASA operates the RQ-4A). The model's proportions differ from published figures, so its length reads 17.4 m against a published 13.5 m.
- `Scripts/import_global_hawk.py` creates `SM_GlobalHawk` (four LODs) and the approved `DA_GlobalHawk` aircraft profile, with a 180° visual yaw. It flies at a scenery speed of 60 m/s around 650 m altitude, turns at 10°/s, and uses the jet loop.
- Review renders are in `Saved/LivingWorld/GlobalHawk-unreal-*.png`.
- A MainLevel PIE check showed Global Hawks and F-4s both spawning, flying and engageable (`main-aircraft-mix.json`).
- No Tripo credits were used.

## Bayraktar TB2 — October 1

`DA_TB2` now uses the Sketchfab model "Baykar Bayraktar TB2" by TheDevilsEye (CC BY 4.0), which you downloaded on October 1. It replaces the Tripo-generated stand-in from September 30.

**Preparation.** `Scripts/prepare_tb2_sketchfab.py`:

- The source is metric: 12.0 m span (published 12 m), 7.5 m long, nose −Y.
- It links the supplied 4K PBR set.
- The three-blade pusher propeller is part of the single mesh. It was located on orthographic renders and a vertex histogram (hub 1.225 m up, blade tips 0.97 m) and weighted to a bone that spins three times a second.

**Flight profile.** 36 m/s at 450 m, piston-engine loop, engageable Plane kind. Credit is recorded in the profile.

The discarded Tripo generations (text task `22764115…`, image task `502a3497…`) remain in the credit ledger; the generated files stay in `Generated/TB2` as history.

## Shahed-136 — October 1

`DA_Shahed136` uses your `shahed-136-drone.zip`, a Tripo-generated OBJ with vertex colours: 1.95 M triangles and no textures. It replaces the Geranium-2. The Geranium's prepared files were deleted, and it was removed from `prepare_living_assets.py` and the manifest; it never had a Living World profile.

**Conversion.** `Scripts/prepare_shahed.py`:

- decimates the mesh to 60k triangles, keeping the vertex colours
- turns the nose to −Y, since the source stands on its nose
- scales to the published 2.5 m span; the generated proportions give 2.56 m length against the published 3.5 m
- weights the pusher propeller to a spinning bone

Unreal shows the colours through `M_VertexColour`.

**Flight profile.** Drone kind, so it flies in the **Drones** count with the quadcopters. It cruises at 50 m/s (published cruise about 185 km/h) at 150 m, with 12°/s turns and a 35° bank limit, and uses the small-engine loop. Engageable with drone health.

## FPV strike drone — October 1

`DA_FPVDrone` uses your `fpv-drone.zip`: a metric OBJ in separate parts, about 0.48 m across the propellers, carrying an RPG-style warhead, with two PBR texture sets.

**Preparation.** `Scripts/prepare_fpv_drone.py`:

- rebuilds the colour and normal materials
- makes four propeller bones at the propeller centres; diagonal pairs counter-rotate
- drops a magenta placeholder part

**Flight profile.** Drone kind. 28 m/s at 35 m, agile (60°/s turns, 40° bank), with the drone loop.

**Import and check.** `Scripts/import_air_batch.py` imports all three with four LODs:

| Model | LOD0 vertices |
|---|---|
| TB2 | 7,689 |
| Shahed-136 | 29,999 |
| FPV drone | 47,094 |

`Scripts/test_main_new_assets.py` confirmed that every profile spawns, flies, is engageable and is photographed in the live MainLevel (`NewAssets-*.png`).

**Licences.** On 30 September you confirmed that you hold the rights to the supplied aircraft models, including these downloads.
