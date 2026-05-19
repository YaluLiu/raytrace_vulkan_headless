# Raster Renderer

The raster target provides a compact Vulkan raster baseline for Hydra and
standalone rendering. It renders mesh color, normalized depth, primitive ID, and
instance ID AOVs through the offscreen framebuffer owned by `PreviewRasterPipeline`.

## Main Files

- `raster_session.cpp` owns Vulkan context setup, resource creation, resize
  forwarding, and frame pass ordering.
- `raster_renderer.cpp` owns renderer setup, render settings, dynamic frame
  uniform slots, resize/lifecycle forwarding, and AOV export wrappers.
- `preview_raster_pipeline.hpp` and `preview_raster_pipeline.cpp` own the main raster
  offscreen AOV targets, graphics pipeline, framebuffer, AOV export backing,
  and draw command recording.
- `raster_descriptor_sets.cpp` keeps descriptor layout updates and renderer
  entry points that route the main preview pass into `PreviewRasterPipeline`.
- `tile_config.hpp`, `tile_aov_atlas.hpp`, `multiview_tile_targets.hpp`,
  `multiview_tile_raster_pipeline.hpp`, and `tile_atlas_renderer.cpp` implement
  multi-camera tile atlas rendering. Tile output renders multiview batches into
  layered images, copies layers into the atlas, then exposes atlas color/depth
  through Hydra AOV export.
- `raster_geometry_upload.cpp` owns mesh upload, geometry refresh, and per-instance
  transform or visibility updates.
- `raster_runtime_updates.cpp` applies Hydra camera, light, material, and scene
  updates before each frame.
- `shaders/raster.vert` and `shaders/raster.frag` are the active shader entry
  points for mesh rendering.
- `shaders/host_device.h` defines the shared host/device buffer and push
  constant layout.

## Frame Flow

`RasterSession::render()` executes three ordered passes:

1. `UpdateUniforms`
2. `UpdateLights`
3. `RenderPreviewAovs`, which first runs `RasterRenderer::renderTileAovAtlas()`
   when tile output is enabled, then runs `RasterRenderer::renderPreviewAovs()` for the
   main output

The raster pipeline binds the shared scene descriptor set, then draws each
visible instance with `PushConstantRaster` data for model transform, object
index, and instance ID. The main frame uniform uses slot 0; tile cameras are
uploaded through the dedicated `TileFrameUniforms` buffer used by the multiview
shaders.

When tile output is enabled, the raster renderer renders cameras from the
current camera array into layered multiview tile targets, then copies each layer into the
row-major atlas. It does not save tile color or tile depth files locally; Hydra
consumes the atlas through tile AOV render buffers.

The atlas size is controlled only by tile configuration:
`tileCameraWidth * gridColumns` by `tileCameraHeight * gridRows`.

## AOVs

`RasterRenderer::GetAovTexture()` exposes the current offscreen Vulkan images for:

- `RasterAov::Color`
- `RasterAov::Depth`
- `RasterAov::PrimId`
- `RasterAov::InstanceId`
- `RasterAov::TileColor`
- `RasterAov::TileDepth`
- `RasterAov::TileDisplayColor`
- `RasterAov::TileDisplayDepth`

Hydra consumes these handles through `hdRobot/hydraRasterAovCopy.cpp` and
`hdRobot/glInteropCache.cpp`. Fixed tile AOVs use exact atlas-sized render
buffers, while display tile AOVs can be scaled to the viewport-sized render
buffer. Material texture asset byte export lives separately in
`hdRobot/hydraTextureAssetExport.cpp`.
