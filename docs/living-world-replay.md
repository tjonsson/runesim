# Population recording and visual replay

Open the Living World panel (F9) and its **Simulation** tab. **Record population** starts sampling the currently active population. **Stop and save** writes a timestamped JSONL file under `Saved/LivingWorld/Recordings`. **Overlay last recording** creates animated, collision-free copies; **Clear replay** removes those copies. Disable ambient activity for an isolated replay view.

The latest recording name is retained for the current session. Recordings persist on disk. A Blueprint or C++ caller can load an earlier name with `SimReplay.LoadRecording`, then call `Play`, `Pause`, `Seek`, or `Clear`. Playback rate is bounded between 0.05 and 8; looping is optional. Positions, orientations, scales, visibility, visual mesh transforms and sampled skeletal clip positions are interpolated or applied to inert components. The original actors are never repositioned or commanded.

Recording has a 16 MiB memory limit. At the limit, sampling stops; use **Stop and save recording** to retain the captured portion. The loader rejects malformed, unordered, non-finite and oversized recordings, duplicate actor IDs, and unsupported asset paths. A failed load leaves the current replay intact. Names are sanitized and files stay in the recording directory.

This is visual inspection of sampled poses. It does not resimulate physics, reproduce Niagara effects/audio, or replay external flight-controller commands. An actor introduced after recording starts is not automatically added to that recording's subject list.

Verification on September 29: the replay validation/interpolation automation suite passed; the live integration exercised jeep/helicopter/gull mesh and animation preservation, bounded seeking and live-actor isolation. The packaged menu also saved and played a 120-agent recording. Evidence is in `Saved/LivingWorld/replay-integration.json` and `packaged-replay.png`.

The movement update also records Blend Space inputs and interpolates them during playback. Older recordings default to zero blend input. A fresh ten-check integration passed with jeep, Huey, gull, blended civilian and crow subjects. This preserves the visual blend input; it is not deterministic reconstruction of every internal animation weight.

Vehicle recordings now also store each wheel's bone name, steering angle and component-space suspension offset. The inert jeep replay uses the same wheel animation proxy and interpolates matching bones. Loader validation rejects duplicate bones, more than 16 wheel entries, non-finite values, angles beyond 60 degrees and offsets beyond 100 cm; older recordings remain clip-only. See [vehicle articulation](living-world-vehicles.md).
