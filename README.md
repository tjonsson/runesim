# RUNE Sim — Realistic UAV Navigation Environment

**RUNE Sim** is a real-time Digital Twin visualization environment for both simulated (SITL) and real (HITL) drone operations, built on Unreal Engine 5.7. It connects to the **Control Room** implemented in this [repository](https://github.com/manudelu/ws_uspace_control_room.git), receives live MQTT telemetry via WebSocket, and renders fleet activity over Cesium 3D geospatial terrain using AirSim physics. And also uses Gaussian Splatting to enhance the rendering capabilities of the operational area in which the drone is moving.

---

## Engine Version

**Unreal Engine 5.7** is required. Download and install it from the [Epic Games Launcher](https://www.unrealengine.com/en-US/download).

To produce a packaged Shipping build, run `build.bat` from the project root. Pass `clean` as the first argument to force a full rebuild. On success, build.bat lists available scenes.

### Build & Run

```
build.bat                Build all scenes (Shipping)
build.bat clean          Full rebuild
build.bat debug          Development build with debug symbols

run.bat                  Launch MainLevel (Cesium, default)
run.bat SecondaryLevel   Launch SecondaryLevel
run.bat <SceneName>      Launch any cooked scene by name
```

New scenes are automatically discovered from `Content/*.umap` and cooked via `-allmaps`. To add a scene, duplicate MainLevel, customise the environment, and save the new `.umap` in Content/. Add it to `+MapsToCook` in `Config/DefaultGame.ini` if `-allmaps` is not used.

---

## Required Plugins

| Plugin | Purpose | Installation Status | 
|--------|--------|--------|
| **[Cosys-AirSim](https://cosys-lab.github.io/Cosys-AirSim/)** | Core simulation backend | Already included in this repository |
| **[Cesium for Unreal](https://www.fab.com/listings/76c295fe-0dc6-4fd6-8319-e9833be427cd)** | Streaming real-world 3D terrain | Install manually via Fab Marketplace |
| **[XScene-UE (Gaussian Splatting)](https://github.com/xverse-engine/XScene-UEPlugin)** | Enables 3D Gaussian Splatting rendering for high-fidelity environment scenes | Requires manual install |

> After installing Cesium, enable it under *Edit → Plugins*, then restart Unreal.

---

## Content Structure Overview

| Folder | Purpose/Functionality |
|--------|------------|
| `Drone/DroneModel/` | Contains DJI S900 model, propellers, and `BP_MyPawn` - a modified version of AirSim's BP_FlyingPawn, including a cinematic third-person camera and drone mesh override |
| `Drone/LandingPad/` | Blueprint that spawns a temporary collision pad under each drone at startup. This ensures drones do not fall through Cesium tiles, which load without collision initially |
| `Drone/BP_SpectatorCamera` | Free-roam spectator camera. Allows the operator to detach from drones and explore the scene |
| `Drone/TelemetryWidget` | HUD UI widget that displays live telemetry (position, velocity, etc.) for the currently selected drone. Telemetry updates are driven through MQTT → WebSocket → Unreal |
| `Drone/BP_MqttTelemetry` | WebSocket listener blueprint that receives telemetry packets from the Control Room and triggers UI updates. Uses an event dispatcher bound in the Level Blueprint to update the widget state |
| `Sensor/BP_SensorDataReceiver` | Listens for sensor-level MQTT messages (e.g., temperature, gas detection) via WebSocket. Updates 3D floating labels (WB_SensorLabel) and spawns visual anomaly markers (e.g., smoke, fire) when thresholds are exceeded |

---

## Level Blueprint Logic Summary

The Level Blueprint orchestrates runtime logic:
* Automatically spawns Landing Pads at drone spawn positions.
* Binds event dispatchers to update the Telemetry Widget per active drone.
* Handles camera switching between drone follow cameras (`Enter` key — cycles between drones) and various camera views (`C` key).
* Toggles visibility of telemetry UI depending on which drone is currently selected.

### Runtime Hotkeys

| Key | Action |
|-----|--------|
| `F1` | Toggle help |
| `Enter` | Switch drone |
| `P` | Toggle Cinematic / Sensor camera quality |
| `O` | Original chase view |
| `C` | Cycle vehicle cameras |
| `F` | FPV view |
| `B` | Fly-with-me view |
| `G` | Ground observer view |
| `M` | Manual camera view |
| `I` | Front camera view |
| `K` | Backup camera view |
| `-` | No display view |
| `V` | Toggle VR headset (HTC Vive) |
| `Tab` | Toggle report |
| `R` | Toggle recording |
| `T` | Toggle trace |
| `1`/`2`/`3` | Toggle subwindows |
| `0` | Toggle all subwindows |
| `Backspace` | Reset |

---

## C++ Subsystems

| Class | Role |
|-------|------|
| `CameraQualitySubsystem` | Tickable world subsystem that manages Cinematic / Sensor camera quality toggle (`P` key). Applies Lumen GI, exposure, and bloom settings to all AirSim cameras. VR-aware — adjusts VR rendering quality when headset is active |
| `RuneSimModeBootSubsystem` | Reads `-scene=<Name>` from the command line and travels to `/Game/<Name>` at startup. Enables `run.bat <SceneName>` scene selection in packaged builds |
| `RuneSimCesiumGeoreferenceSubsystem` | Reads `OriginGeopoint` from AirSim `settings.json` and repositions the Cesium georeference origin at startup |
| `VRSpectator` | Manages HTC Vive VR headset toggle (`V` key), camera cycling, and VR-specific rendering settings |

---

## C++ Network Layer (WebSocket Receivers)

| Class | Role |
|-------|------|
| `BaseWebSocketReceiver` | Base implementation that manages WebSocket connection, reconnection, and disposal, with retry logic |
| `MqttTelemetryReceiver` | Connects to Control Room WebSocket and forwards drone telemetry to BP_MqttTelemetry |
| `SensorDataReceiver` | Receives raw sensor packets and forwards them to BP_SensorDataReceiver → triggers 3D scene annotations and widget updates |

---

## Importing a Georeferenced Gaussian Splatting Model

This project supports high-fidelity Gaussian Splatting environments using the XScene-UE Plugin. To ensure correct positioning and alignment with Cesium tiles, follow the steps below.

### Prerequisites
Before importing, make sure you have:
* Trained a georeferenced Gaussian Splat following the workflow in [this](https://github.com/manudelu/georeferenced_gsplat.git) repository.
* Exported the .splat or packaged model compatible with XScene-UEPlugin.
* Identified the GPS coordinates (Lat, Lon, Alt) of the first frame / reference image used during training (this serves as the model’s origin).

### Step-by-Step Import into Unreal Engine
*1. Install XScene-UE Plugin*
* Clone or download: https://github.com/xverse-engine/XScene-UEPlugin
* Place it under Plugins/ and restart Unreal Engine
* Enable it via Edit → Plugins → XScene-UE

*2. Import the Gaussian Splatting Model*
* Go to XScene → Import
* Select your trained Gaussian Splat package
* It will appear under Content Browser ➜ XScene

*3. Place It Under Cesium Tiles in World Outliner*
* In the World Outliner, locate Google Photorealistic 3D Tiles
* Drag your Gaussian Splat actor as a child of Cesium’s tileset (This ensures consistent origin & streaming alignment)

*4. Attach a CesiumGlobeAnchor Component*
* Select your Gaussian Splat actor
* Click Add Component → CesiumGlobeAnchor
* Enter the Lat, Lon, Alt of the reference frame (first training image)
* Unreal will reposition & anchor the Gaussian Splat into the correct global coordinate system
