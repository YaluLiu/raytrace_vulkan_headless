# Stable And Temporary Integrations Map

Read this map only when a task explicitly concerns the excluded
`UsdRaySensor/`, `UsdRaySensorImaging/`, or `python/` trees. These files are not
part of the active CodeGraph or graphify indexes.

## USD Sensor Schema

| Files | Responsibility |
| --- | --- |
| `UsdRaySensor/schema.usda` | Code-generation source for typed LiDAR and height-scan schemas. |
| `UsdRaySensor/generatedSchema.usda` | Runtime fallback attributes and schema documentation. |
| `UsdRaySensor/lidarSensor.*`, `heightScanSensor.*` | Generated typed C++ schema APIs and custom scalar getters. |
| `UsdRaySensor/tokens.*` | Generated schema and attribute tokens. |
| `UsdRaySensor/plugInfo.json` | Schema plugin metadata. |
| `UsdRaySensor/CMakeLists.txt` | Schema-only target and install layout. |

Authored prim types remain `LidarSensor` and `HeightScanSensor`; the typed C++
APIs are `UsdGeomLidarSensor` and `UsdGeomHeightScanSensor`.

## USD Imaging Adapters

| Files | Responsibility |
| --- | --- |
| `UsdRaySensorImaging/hydraSensor.*` | Shared `lidarSensor`/`heightScanSensor` sprim tokens and dirty bits. |
| `UsdRaySensorImaging/lidarSensorAdapter.*` | Inserts and updates LiDAR sprims from typed schema attributes. |
| `UsdRaySensorImaging/heightScanSensorAdapter.*` | Inserts and updates height-scan sprims from typed schema attributes. |
| `UsdRaySensorImaging/plugInfo.json` | UsdImaging adapter registration. |
| `UsdRaySensorImaging/CMakeLists.txt` | Imaging plugin target and install layout. |

The adapters bridge schema prims into the sensor sprims implemented by
`hdRobot/lidarSensor.*` and `hdRobot/heightScanSensor.*`.

## Temporary Python Binding

| Files | Responsibility |
| --- | --- |
| `python/robot_raster_py.cpp` | pybind11 module and public Python signatures. |
| `python/depth_camera_provider.*` | Creates a fixed camera batch from a USD camera template, updates batched poses, and returns NumPy `[n,h,w]` depth. |
| `python/height_scan_provider.*` | Loads a training scene, maps environment poses to synthetic sensors, renders, and returns NumPy hit points. |
| `python/depth_camera_demo.py` | Standalone multi-camera depth smoke example. |
| `python/isaac_lab_height_scan_demo.py` | Smoke test and Isaac Lab integration sketch. |
| `python/CMakeLists.txt` | Optional `ROBOT_ENGINE_BUILD_PYTHON=ON` extension target. |

The provider depends on `headlessTrainingCore` and the public `Engine` API.
Treat this directory as temporary: do not move its behavior into the core maps
unless the integration becomes permanent.

## Build Routes

- `bash install.sh schema`: configure/build/install the schema-only plugin.
- `build_windows.bat`: build/install schema, imaging, and Hydra delegate
  components on Windows.
- `bash install.sh python`: build and install the optional Python extension.
- `python3 python/depth_camera_demo.py --usd <scene.usd>`: render a NumPy depth batch after installation.
