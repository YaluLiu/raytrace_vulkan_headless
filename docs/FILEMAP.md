# File Map

This is an optional domain router, not a default pre-read. Use it only when the
owning domain or source entry point is unclear, or when a task crosses modules.
If the task names a directory, file, or symbol, or CodeGraph already identifies
the source and call path, skip this file and inspect that source directly.

## Routing Flow

1. For architecture or cross-module questions, read
   `graphify-out/GRAPH_SUMMARY.md` before using this router.
2. Select one domain map under `docs/filemap/`; add a second only when the path
   crosses a module boundary.
3. Search only the relevant `GRAPH_REPORT.md` section, or traverse a bounded
   `graph.json` subgraph, when the task needs deeper relationship context.

## Domain Maps

| Domain | Read when the task concerns | Detailed map |
| --- | --- | --- |
| Engine | Vulkan lifecycle, scene GPU state, AOVs, cameras, tiles, LiDAR, height scan, shaders | [`filemap/ENGINE.md`](filemap/ENGINE.md) |
| Hydra delegate | Hydra sync, scene-store ownership, materials, render buffers, GL interop, delegate settings | [`filemap/HDROBOT.md`](filemap/HDROBOT.md) |
| Headless training | Batch generation, viewer, USD scene loading, physics frames, output writers | [`filemap/HEADLESS_TRAINING.md`](filemap/HEADLESS_TRAINING.md) |
| Stable or temporary integrations | USD schema/adapters or the optional Python binding | [`filemap/STABLE_INTEGRATIONS.md`](filemap/STABLE_INTEGRATIONS.md) |

## Cross-Domain Runtime Paths

- Hydra: `HdRenderPass::_Execute` → `hdrobot::PassBridge` →
  `hdrobot::EngineSession` → public `Engine` API.
- Headless batch: `main` → `RunTraining` → `TrainingSceneRuntime` → public
  `Engine` API → output writers.
- Training viewer: `viewer_main` → `ViewerApp` → `TrainingSceneRuntime` →
  public `Engine` API → ImGui display/debug export.
- Physics updates: `PhysicsFrameSource` → `PhysicsFabric` →
  `ApplyLatestFabricFrame` → `TrainingSceneRuntime::applyPoseUpdates`.

## First-Hop Routing

| Question | Read first | Initial files or symbols |
| --- | --- | --- |
| Frame pass order | Engine | `frame_executor.cpp`, `OutputController`, `recordFramePasses` |
| Engine setup, teardown, or resize | Engine | `engine_session.cpp`, `engine_facade.cpp`, `RenderResourceLifecycle` |
| GPU mesh, texture, instance, light, or TLAS state | Engine | `engine_scene.cpp`, `GpuScene`, focused scene stores |
| Preview or tile AOV generation | Engine | `PreviewPipeline`, `TileAtlasPass`, `GetAovTexture` |
| Hydra AOV copy or render-buffer export | Hydra + Engine | `aovBridgeSpec.cpp`, `hydraAovCopy.cpp`, engine AOV contracts |
| Hydra scene synchronization | Hydra | `RenderDelegate::CommitResources`, `EngineSceneSync`, `SceneStore` |
| Hydra materials or textures | Hydra + Engine | `materialXParser.cpp`, `material.cpp`, `TextureStore` |
| Camera or multi-camera behavior | Relevant caller + Engine | `CameraSpec`, `camera_projection.cpp`, `view_uniforms.cpp` |
| LiDAR generation or overlay | Engine; add Hydra for discovery | `LidarPointCloudPass`, `lidar_scan.cpp`, `lidarSensor.cpp` |
| Height-scan generation or overlay | Engine; add caller map | `HeightScanPass`, `height_scan.cpp`, `RtScene` trace masks |
| Headless batch behavior | Headless | `train/main.cpp`, `training_runner.cpp`, `TrainingSceneRuntime` |
| Training viewer behavior | Headless + Engine | `viewer_app.cpp`, camera controller, viewer output config |
| USD scene/material/sensor ingestion | Headless | `usd_scene_loader.cpp` and the focused `usd_*_reader.cpp` |
| Physics replay or frame handoff | Headless | `PhysicsFrameSource`, `PhysicsFabric`, `ApplyLatestFabricFrame` |
| USD schema or imaging adapter behavior | Stable integrations | `schema.usda`, generated schema APIs, imaging adapters |
| Python/Isaac Lab binding | Stable integrations + Headless | `DepthCameraProvider`, `HeightScanProvider`, `TrainingSceneRuntime` |
| Build or install failure | Relevant map | Root and target `CMakeLists.txt`, `install.sh`, failing output |
