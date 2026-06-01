# Engine Runtime

The engine target provides a compact Vulkan graphics baseline for Hydra and
standalone rendering. It renders mesh color, normalized depth, primitive ID, and
instance ID AOVs through the offscreen framebuffer owned by `PreviewPipeline`.

## Main Files

- `include/engine/` contains the public engine API consumed by `hdRobot/`.
- `src/core/` contains `Engine` facade support, private render-resource
  helpers, frame pass ordering, and output orchestration.
- `src/runtime/` contains the engine-owned Vulkan context lifecycle and
  headless app context.
- `src/scene/` owns mesh, material, instance, light, texture, descriptor, and
  view uniform resources shared by all outputs.
- `features/preview/` owns the main graphics offscreen AOV targets, graphics
  pipeline, framebuffer, AOV export backing, draw command recording, and
  preview shaders.
- `features/tile/` implements multi-camera tile atlas rendering and owns the
  tile shaders. Tile output renders multiview batches into layered images,
  copies layers into the enabled and requested per-channel atlases, then exposes
  atlas color/depth through Hydra AOV export.
- `features/lidar/` implements LiDAR scan layout, angular range graphics projection,
  GPU point generation, CPU point-cloud readback, preview color point overlay,
  and LiDAR shaders.
- `features/height_scan/` implements height-scan sample layout, GPU sample
  generation, CPU grid readback, preview overlay, and height-scan shaders.
- `features/point_overlay/` contains the shared preview point overlay pipeline
  used by LiDAR and height scan.
- `shaders/common/host_device.h` defines the shared host/device buffer and push
  constant layout used by C++ and GLSL feature code.

## Frame Flow

`Engine::render()` owns command buffer begin/end/submit and delegates
pass ordering to `engine::recordFramePasses()`, which executes:

1. `UpdateUniforms`
2. `UpdateLights`
3. `RenderTileAovAtlas`
4. `GenerateLidarPointClouds`
5. `GenerateHeightScans`
6. `RenderPreviewAovs`
7. `OverlayLidarPointCloud`
8. `OverlayHeightScans`

The graphics pipeline binds the shared scene descriptor set, then draws each
visible instance with `MeshDrawPushConstants` data for model transform, object
index, and instance ID. The main frame uniform uses slot 0; tile cameras are
uploaded through the dedicated `TileFrameUniforms` buffer used by the multiview
shaders.

When tile output is enabled, the engine renderer renders cameras from the
current camera array into layered multiview tile targets, then copies each layer
into the row-major color and/or depth atlases requested for the current frame.
Tile color and depth channels can be enabled independently through tile config
and are also gated by the bound Hydra tile AOVs. When both channels are active,
they still share one multiview graphics pass and do not repeat geometry
draws. Tile output does not save tile color or tile depth files locally; Hydra
consumes the atlas through tile AOV render buffers.

The atlas size is controlled only by tile configuration:
`tileCameraWidth * gridColumns` by `tileCameraHeight * gridRows`.

LiDAR output is a sensor-specific point-cloud contract, not a `Aov`.
Hydra forwards sorted LiDAR sensors and visualization settings to
`Engine`; engine generates a global GPU point buffer plus per-sensor
metadata. CPU consumers use `Engine::readLidarPointCloudFrame()`.
When visualization is enabled, the selected sorted sensor index is drawn as red
points over the preview color attachment before Hydra copies the color AOV.

## Extension Points

Future simulation sensor outputs should start at the output layer:

- Add feature implementation files and shaders under `features/<sensor>/`.
- Extend `OutputController` first, reusing `GpuScene`,
  `SceneDescriptors`, and `ViewUniforms` instead of duplicating
  scene, descriptor, or uniform ownership.
- Add a public config or output contract under `include/engine/` only when an
  external caller needs it.
- Extend `Aov` only for outputs that need the existing AOV texture query
  path; use a focused contract for sensor outputs with different semantics.

Do not let a sensor pass own scene data, introduce Hydra types into `engine/`,
or copy descriptor/uniform management logic.

Hydra-specific token, render setting, and render buffer copy adaptation belongs
in `hdRobot/`; engine components should remain free of OpenUSD Hydra types.

## AOVs

`Engine::GetAovTexture()` exposes the current offscreen Vulkan images for:

- `Aov::Color`
- `Aov::Depth`
- `Aov::PrimId`
- `Aov::InstanceId`
- `Aov::TileColor`
- `Aov::TileDepth`

Hydra consumes these handles through `hdRobot/hydraAovCopy.cpp` and
`hdRobot/glInteropCache.cpp`. Fixed tile AOVs use exact atlas-sized render
buffers, while display tile Hydra tokens are bridge-side copy policies mapped
to the same engine tile atlas handles and can be scaled to the viewport-sized
render buffer. Material texture asset byte export lives separately in
`hdRobot/hydraTextureAssetExport.cpp`.
