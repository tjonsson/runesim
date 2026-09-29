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
