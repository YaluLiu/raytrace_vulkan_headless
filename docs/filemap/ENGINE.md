# Engine Domain Map

Read this map for work under `engine/`. Return to `docs/FILEMAP.md` before
loading another domain map.

## Public Contracts

| File | Responsibility |
| --- | --- |
| `engine/include/engine/engine.hpp` | Public `Engine` facade for lifecycle, scene updates, output configuration, rendering, readback, and AOV access. |
| `engine/include/engine/renderer_types.hpp` | Camera and facade-facing runtime types. |
| `engine/include/engine/output_config.hpp` | Snapshot configuring tile, LiDAR, and height-scan outputs. |
| `engine/include/engine/mesh_types.hpp` | CPU mesh geometry, vertex, and material upload contracts. |
| `engine/include/engine/texture_asset.hpp` | Encoded texture payload, usage, and color-space contract. |
| `engine/include/engine/aov_texture.hpp` | Exported engine AOV enum and Vulkan texture descriptor. |
| `engine/include/engine/tile_config.hpp` | Tile atlas dimensions, channels, and normalization. |
| `engine/include/engine/lidar_types.hpp` | LiDAR sensor, visualization, sample, and readback types. |
| `engine/include/engine/height_scan_types.hpp` | Height-scan sensor, grid, sample, and readback types. |

## Lifecycle And Frame Orchestration

| Files | Responsibility |
| --- | --- |
| `engine/src/runtime/engine_session.cpp`, `headless_vk.*` | Vulkan context ownership, command submission, resize, and headless setup. |
| `engine/src/core/engine_facade.cpp` | Public facade forwarding and AOV routing. |
| `engine/src/core/renderer_internal.hpp` | Private access to the `Engine` implementation and owned subsystems. |
| `engine/src/core/renderer_resources.*` | Resource create/destroy/resize wrappers. |
| `engine/src/core/render_resource_lifecycle.*` | Dependency-ordered creation, shutdown, descriptor refresh, and pipeline rebuilds. |
| `engine/src/core/frame_executor.*` | Source of truth for per-frame pass sequencing. |
| `engine/src/core/output_controller.*` | Owns and dispatches preview, tile, LiDAR, and height-scan outputs. |

The main frame sequence is preview AOV rendering, LiDAR generation/overlay,
height-scan generation/overlay, and requested readback/export work.

## Scene And GPU Ownership

| Files | Responsibility |
| --- | --- |
| `engine/src/scene/engine_scene.cpp` | Public scene facade implementation and resource-rebinding coordination. |
| `engine/src/scene/gpu_scene.*` | Aggregates scene stores and drives ray-tracing resource lifecycle. |
| `engine/src/scene/mesh_store.*` | Source geometry, GPU mesh/material buffers, and object descriptions. |
| `engine/src/scene/texture_store.*` | Decode/upload, fallback texture, rebuild, and descriptors. |
| `engine/src/scene/instance_store.*` | Renderer instances and external instance IDs. |
| `engine/src/scene/light_store.*` | Light records and GPU light buffer. |
| `engine/src/scene/rt_scene.*` | BLAS/TLAS build/update and normal-versus-ground trace masks. |
| `engine/src/scene/scene_descriptors.*` | Frame, scene, light, tile, TLAS, and texture descriptor bindings. |
| `engine/src/scene/view_uniforms.*` | Main/tile cameras and dynamic uniform buffers. |
| `engine/src/scene/camera_projection.*` | `CameraSpec` projection and conform-policy calculations. |

## Output Features

| Feature | First files |
| --- | --- |
| Preview AOVs | `engine/features/preview/preview_pipeline.*` |
| Tile atlas | `engine/features/tile/tile_atlas_pass.*`, `tile_aov_channel.*` |
| Tile multiview | `engine/features/tile/multiview_tile_targets.*`, `multiview_tile_pipeline.*` |
| LiDAR CPU layout | `engine/features/lidar/lidar_scan.*` |
| LiDAR GPU generation/readback | `lidar_point_cloud_pass.*`, `lidar_point_generation_pipeline.*` |
| Height-scan CPU layout | `engine/features/height_scan/height_scan.*` |
| Height-scan GPU generation/readback | `height_scan_pass.*`, `height_scan_generation_pipeline.*` |
| Shared sensor GPU mechanics | `engine/features/sensor_common/` |
| Shared point overlays | `engine/features/point_overlay/point_overlay_pipeline.*` |

## Shader And ABI Sources

- `engine/shaders/common/host_device.h`: shared host/shader structs, bindings,
  trace masks, frame uniforms, and push constants.
- `engine/features/preview/shaders/mesh.vert` and `mesh.frag`: preview mesh
  transform, lighting, material sampling, and AOV writes.
- `engine/features/tile/shaders/`: tile multiview mesh and dome-background
  variants.
- `engine/features/lidar/shaders/`: ray-query generation and preview overlay.
- `engine/features/height_scan/shaders/`: ground-mask ray queries and overlay.
- `engine/features/preview/shaders/viewer_display_color.comp`: viewer-only
  vertical flip and linear-to-sRGB conversion.

## Focused Tests

- `engine/tests/camera_projection_test.cpp`
- `engine/tests/height_scan_basis_test.cpp`

## Question Routes

| Topic | Start with | Search symbols |
| --- | --- | --- |
| Setup/teardown | `engine_session.cpp`, `render_resource_lifecycle.cpp` | `setup`, `createRenderResources`, `destroyResourcesInShutdownOrder` |
| Resize | `engine_facade.cpp`, `preview_pipeline.cpp` | `onResize`, `resizeRenderTargets`, `recreateAovTargets` |
| Frame order | `frame_executor.cpp`, `output_controller.cpp` | `recordFramePasses`, `recordPreviewAovs`, `recordTileAtlas` |
| AOV access | `aov_texture.hpp`, `output_controller.cpp` | `GetAovTexture`, `getAovTexture` |
| Camera projection | `camera_projection.cpp`, `view_uniforms.cpp` | `BuildCameraProjection`, `setMainCamera`, `setCameras` |
| Mesh upload/update | `engine_scene.cpp`, `gpu_scene.cpp`, `mesh_store.cpp` | `uploadMesh`, `updateMeshGeometry`, `updateMaterialsAtRuntime` |
| TLAS/trace masks | `rt_scene.cpp` | `markTlasDirty`, `flush`, `kTraceMaskGround` |
| LiDAR | `lidar_point_cloud_pass.cpp`, `lidar_scan.cpp` | `LidarPointCloudPass`, `BuildLidarScanLayout` |
| Height scan | `height_scan_pass.cpp`, `height_scan.cpp` | `HeightScanPass`, `BuildHeightScanBasis` |
| Shader material ABI | `mesh_types.hpp`, `host_device.h`, `mesh.frag` | `WaveFrontMaterial`, `sampleBaseColor` |
