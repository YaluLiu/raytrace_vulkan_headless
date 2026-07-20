# Headless Training Domain Map

Read this map for batch generation, the debug viewer, USD ingestion, physics
frame handoff, and output serialization under `headlessTraining/`.

## Executable Flows

| Files | Responsibility |
| --- | --- |
| `headlessTraining/train/main.cpp` | Thin batch CLI entry; parses options and calls `RunTraining`. |
| `headlessTraining/train/cli.*` | Batch arguments, validation, output directory, cameras, preview, and exports. |
| `headlessTraining/train/training_runner.*` | Batch orchestration from engine setup through per-frame render/readback/output. |
| `headlessTraining/viewer/viewer_main.cpp` | Viewer CLI entry and `ViewerApp` startup. |
| `headlessTraining/viewer/viewer_app.*` | Window/Vulkan context, playback, engine lifecycle, ImGui viewport, and debug actions. |
| `headlessTraining/viewer/viewer_cli.*` | Viewer-specific arguments. |
| `headlessTraining/viewer/viewer_camera_controller.*` | Orbit camera and scene-focus behavior. |
| `headlessTraining/viewer/viewer_output_config.*` | Sensor compute versus overlay visibility and sensor selection. |
| `headlessTraining/viewer/viewer_export.*` | Unique screenshot, LiDAR CSV, and height-scan CSV debug paths. |

The batch path is `main` → `RunTraining` → scene/physics update →
`Engine::render` → sensor readback → output writers. The viewer shares core
scene and frame-application code but owns presentation and explicit exports.

## Shared Runtime Model

| Files | Responsibility |
| --- | --- |
| `headlessTraining/core/training_scene.*` | CPU scene model, engine upload, output configuration, pose updates, and lookup indexes. |
| `headlessTraining/core/preview_camera.*` | Bounds-based default preview camera with gravity-aligned overhead behavior. |
| `headlessTraining/core/output_writers.*` | LiDAR/height-scan CSV, manifest JSON, escaping, and frame naming. |

`TrainingSceneRuntime` is the boundary between loaded/updated CPU state and
the public `Engine` API.

## USD Scene Ingestion

| Files | Responsibility |
| --- | --- |
| `headlessTraining/core/usd_scene_loader.*` | Stage traversal, requested-camera validation, and final scene assembly. |
| `headlessTraining/core/usd_scene_reader_utils.*` | Vectors, matrices, cameras, visibility, trace masks, attribute reads, and sanitization. |
| `headlessTraining/core/usd_mesh_reader.*` | Topology, triangulation, normals, UVs, transforms, materials, and trace roles. |
| `headlessTraining/core/usd_material_reader.*` | Basic UsdPreviewSurface values, textures, and display-color fallback. |
| `headlessTraining/core/usd_texture_registry.*` | Asset resolution, byte extraction, deduplication, and color space. |
| `headlessTraining/core/usd_sensor_reader.*` | LiDAR/height-scan detection, defaults, validation, and pose conventions. |
| `headlessTraining/core/usd_light_reader.*` | Dome, sphere, and cylinder light conversion. |
| `headlessTraining/core/usd_animation_source.*` | Time-sampled mesh/sensor pose and visibility updates. |

## Physics Frame Pipeline

| Files | Responsibility |
| --- | --- |
| `headlessTraining/core/physics_frame.*` | `PhysicsFrameSource`, frame snapshot, and advance status contract. |
| `headlessTraining/core/physics_fabric.*` | Single-latest-frame publish/consume handoff. |
| `headlessTraining/core/usd_physics_replay_source.*` | `PhysicsFrameSource` backed by USD transform/visibility time samples. |
| `headlessTraining/core/training_frame_consumer.*` | Applies the latest fabric frame to `TrainingSceneRuntime` and reports sensor-output dirtiness. |

The shared path is `PhysicsFrameSource::nextFrame` → `PhysicsFabric::publish`
→ `ApplyLatestFabricFrame` → `TrainingSceneRuntime::applyPoseUpdates`.

## Tests

| Test | Coverage |
| --- | --- |
| `headlessTraining/tests/usd_scene_loader_test.cpp` | Mesh, transform, camera, sensor defaults, sanitization, and trace roles. |
| `headlessTraining/tests/usd_animation_output_test.cpp` | Animation source, matrix conversion, CSV, and manifest output. |
| `headlessTraining/tests/physics_fabric_test.cpp` | Latest-frame fabric semantics. |
| `headlessTraining/tests/training_frame_consumer_test.cpp` | Frame application and sensor-output dirty results. |
| `headlessTraining/tests/cli_test.cpp` | Batch option parsing and validation. |
| `headlessTraining/tests/preview_camera_test.cpp` | Automatic camera fitting and overrides. |
| `headlessTraining/tests/viewer_camera_controller_test.cpp` | Authored-camera orientation and projection preservation. |
| `headlessTraining/tests/fixtures/smoke_scene.usda` | Minimal executable smoke scene. |

## Question Routes

| Topic | Start with | Search symbols |
| --- | --- | --- |
| Batch orchestration | `train/main.cpp`, `training_runner.cpp` | `RunTraining`, `Engine::render`, readback writers |
| Batch CLI | `train/cli.cpp` | `ParseCommandLine`, `TrainingOptions` |
| Viewer runtime | `viewer_main.cpp`, `viewer_app.cpp` | `ViewerApp`, `runFrame`, `applyOutputConfigIfDirty` |
| Viewer camera | `viewer_camera_controller.cpp`, `preview_camera.cpp` | `ViewerCameraController`, `BuildPreviewCamera` |
| Viewer exports | `viewer_export.cpp` | `ExportViewerScreenshot`, CSV export helpers |
| Scene upload/updates | `training_scene.cpp` | `uploadToEngine`, `configureEngineOutputs`, `applyPoseUpdates` |
| USD loading | `usd_scene_loader.cpp`, focused readers | `LoadUsdTrainingScene`, `ReadMesh`, `ReadSensor` |
| Animation | `usd_animation_source.cpp` | `LoadUsdAnimationSource`, sampled pose updates |
| Physics replay | `usd_physics_replay_source.cpp`, `physics_frame.h` | `LoadUsdPhysicsReplaySource`, `nextFrame`, `reset` |
| Frame handoff | `physics_fabric.cpp`, `training_frame_consumer.cpp` | `publish`, `consumeLatest`, `ApplyLatestFabricFrame` |
| Output files | `output_writers.cpp` | `WriteLidarCsv`, `WriteHeightScanCsv`, `WriteManifestJson` |
