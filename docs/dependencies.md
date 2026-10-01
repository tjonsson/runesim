# Dependencies and repository policy

RuneSim targets Unreal Engine **5.8**; the current Windows build was verified with **5.8.2** and Visual Studio 2022 C++ tools. Install Git LFS before cloning, then run `git lfs install` and `git lfs pull`. Unreal assets and editable binary art sources use LFS.

## Unreal plugins

| Dependency | Tested version | Required installation |
|---|---|---|
| Cosys-AirSim | Project's checked-in integration | Included under `Plugins/AirSim`. Keep this source integration and its third-party libraries; replacing it with an arbitrary upstream release can change behavior. |
| [Cesium for Unreal](https://www.fab.com/listings/76c295fe-0dc6-4fd6-8319-e9833be427cd) | 2.29.1 for UE 5.8 | Required. Install from Fab into the matching engine, or place the official plugin in `Plugins/CesiumForUnreal`. Configure your own Cesium access in the editor. |
| [Tripo DCC Bridge](https://studio.tripo3d.ai) | Unreal bridge 1.0.5, UE 5.8 Win64 | Optional, editor only. Download through Studio's DCC Bridge installer and extract to `Plugins/Tripo3DUEBridge`. Its absence does not block normal simulation. See [bridge setup](tripo-bridge-setup.md). |
| Pixel Streaming 2 / Pixel Capture | Bundled with UE 5.8 | Enabled engine plugins. Do not copy their engine source or content into the repository. |
| Niagara, NiagaraFluids, JSON utilities, OpenXR, Modeling Tools | Bundled with UE 5.8 | Use the engine-provided versions selected by `RuneSim.uproject`. NiagaraFluids drives the close-range fire simulation (see [battlefield effects](living-world-war-effects.md)). |
| [XScene-UE](https://github.com/xverse-engine/XScene-UEPlugin) / XV3dGS | Not validated by Living World | Optional Gaussian-splatting workflow; disabled by default. Install separately only when needed. |
| ObjectDeliverer | Not required | Disabled in the project. Do not vendor a store download. |

`Plugins/` ignores every plugin except the existing AirSim integration. Fab/store packages and vendor bridge downloads stay on the workstation, even when installed at project scope. Project-owned plugins can be deliberately allowlisted later. This rule does not delete installed plugins or rewrite Git history.

## Asset tools and streaming

- Blender **5.2.1 LTS** was used to prepare the editable art. The optional Blender Tripo bridge was **1.0.34**. Neither application nor add-on is redistributed here.
- Python **3.12** runs the host-side utilities. Unreal's editor scripts use its bundled Python. Script-specific temporary environments and reports belong under ignored `Saved/`.
- `Scripts/setup_signalling.ps1` prepares Epic's Pixel Streaming signalling infrastructure under ignored `Saved/LivingWorld/Signalling`; see [packaged demo setup](living-world-packaged-demo.md). If the installed engine's `Engine/Plugins/Media/PixelStreaming2/Resources/WebServers` folder contains only downloader scripts, run its official `get_ps_servers.bat` first. Copied Epic downloader files under `Samples/PixelStreaming2` are also excluded from Git.
- Pi streaming uses ROS2 Humble and the scripts in `Scripts/pi_stream_test`; see [Pi setup](pi-stream-test.md). Hostnames and URLs in the examples describe the development LAN and must be adjusted for another installation. Do not commit passwords, tokens or private keys.

## What belongs in Git

Commit project source, configuration, scripts, documentation, sample data, Unreal content and editable art sources. Keep attribution/provenance alongside sourced assets. User-supplied aircraft have unverified redistribution licenses; this repository does not establish or grant rights to those third-party models. Review those rights before further redistribution or commercial use.

Builds, cooked/staged output, caches, test recordings, machine-local settings, virtual environments, Blender backups and downloaded plugins are ignored. Keep the local packaged build and raw test reports under `Saved/`; the docs summarize their results. Existing AirSim third-party libraries remain tracked because its build depends on them.

Before committing, run `python Scripts/check_repository.py`, `git diff --check`, and inspect `git status`. The check rejects tracked external plugins, build/cache files and missing LFS attributes for project binary assets.
