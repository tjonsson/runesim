# Tripo Studio bridge setup

Verified on 2026-09-28 on Tommy's workstation.

## Installed components

| Destination | Application | Bridge | Local listener |
|---|---|---|---|
| Blender | 5.2.1 LTS | 1.0.34 | 127.0.0.1:60600 |
| RuneSim Unreal Editor | 5.8.2 | 1.0.5, UE 5.8 Win64 build | 127.0.0.1:60620 |

Official packages were downloaded using Tripo Studio's DCC Bridge install links. Studio's displayed version labels lag behind the installed package metadata.

- Blender add-on: `C:\Users\tommy\AppData\Roaming\Blender Foundation\Blender\5.2\scripts\addons\Tripo3d_Blender_Bridge`. Enabled and preferences saved.
- Unreal project plugin: `Plugins/Tripo3DUEBridge`. Enabled for the editor in `RuneSim.uproject`. Its binary build ID matches the installed Unreal Editor.
- Downloads: `C:\Users\tommy\Downloads\Tripo3d_Blender_Bridge-latest.zip` and `Tripo3d_UE_Bridge-latest.zip`.

## Reconnect

1. Open Blender with the add-on enabled, or open RuneSim in Unreal and keep **Window > Tripo Bridge** open.
2. Open [Tripo Studio](https://studio.tripo3d.ai) in Chrome.
3. Open **DCC Bridge** and enable the desired destination. Both destinations were verified separately; switching destination disconnected the previous one during testing.
4. Confirm **Connected**, then select a model and use **Export > Send To**. Inspect the displayed credit cost before exporting.

## Verification

- Blender: Studio displayed Connected, and the add-on reported state `connected` with client `Tripo Studio`.
- Unreal: Studio displayed Connected with Unreal enabled; `Saved/Logs/RuneSim.log` recorded the Tripo Studio WebSocket handshake, protocol 1.1.0.
- Both bridge ports were listening on loopback.
- Screenshot: `Saved/TripoBridgeVerification/studio-unreal-connected.png` (local, ignored by Git).
- Subsequent authorized asset work verified actual textured pigeon and gull transfers to both Blender and Unreal. Both humanoid animation bundles also transferred to Blender. The original connection-only check above was followed by these transfer tests; it is no longer a pending step.

The entire downloaded bridge is excluded from Git. A fresh checkout can run the simulator without it: the editor plugin reference is optional. To use Tripo imports, install the official UE 5.8 package into `Plugins/Tripo3DUEBridge` (including its matching binaries), or build the vendor source for your engine. See [dependency installation](dependencies.md). The local installed copy is retained.

See [Living World implementation status](living-world-implementation.md) for the implemented gameplay, prepared assets, test evidence and remaining production limits. Connections are per Studio tab and running application session; they must be re-established after restarting the destination application.

## Official instructions

- [Unreal bridge](https://www.tripo3d.ai/blog/tripo-dcc-bridge-for-ue)
- [Blender bridge](https://www.tripo3d.ai/blog/tripo-dcc-bridge-for-blender)
