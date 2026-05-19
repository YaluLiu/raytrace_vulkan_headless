# Raster Renderer

The raster target provides a compact Vulkan raster baseline for Hydra and
standalone rendering. It renders mesh color, normalized depth, primitive ID, and
instance ID AOVs through the offscreen framebuffer owned by `PreviewRasterPipeline`.

## Main Files

- `raster_session.cpp` owns Vulkan context setup, resource creation, resize
  forwarding, command buffer begin/end/submit, and renderer lifetime.
- `raster_frame_executor.cpp` is the frame pass ordering source of truth.
- `raster_renderer.cpp` keeps `RasterRenderer` as the stable facade and
  delegates setup, scene, uniform, descriptor, and output work to focused
  components.
- `raster_gpu_scene.hpp` and `raster_gpu_scene.cpp` own mesh, material,
  instance, light, texture, object-description, and light GPU resources.
- `raster_scene_descriptors.hpp` and `raster_scene_descriptors.cpp` own the
  shared scene descriptor bindings, pool, layout, set, and descriptor updates.
- `raster_view_uniforms.hpp` and `raster_view_uniforms.cpp` own camera state,
  main frame uniforms, and tile multiview frame uniforms.
- `raster_output_controller.hpp` and `raster_output_controller.cpp` orchestrate
  output passes and route AOV texture queries.
- `preview_raster_pipeline.hpp` and `preview_raster_pipeline.cpp` own the main raster
  offscreen AOV targets, graphics pipeline, framebuffer, AOV export backing,
  and draw command recording.
- `raster_descriptor_sets.cpp` keeps descriptor layout updates and renderer
  compatibility entry points that route through `RasterSceneDescriptors` and
  `RasterOutputController`.
- `tile_config.hpp`, `tile_aov_atlas.hpp`, `multiview_tile_targets.hpp`,
  `multiview_tile_raster_pipeline.hpp`, `tile_atlas_pass.hpp`, and
  `tile_atlas_renderer.cpp` implement multi-camera tile atlas rendering. Tile
  output renders multiview batches into layered images, copies layers into the
  atlas, then exposes atlas color/depth through Hydra AOV export.
- `raster_geometry_upload.cpp`, `raster_runtime_updates.cpp`, and
  `raster_scene_upload.cpp` are `RasterRenderer` facade forwarding layers for
  scene updates now owned by `RasterGpuScene`.
- `shaders/raster.vert` and `shaders/raster.frag` are the active shader entry
  points for mesh rendering.
- `shaders/host_device.h` defines the shared host/device buffer and push
  constant layout.

## Frame Flow

`RasterSession::render()` owns command buffer begin/end/submit and delegates
pass ordering to `raster::recordFramePasses()`, which executes:

1. `UpdateUniforms`
2. `UpdateLights`
3. `RenderTileAovAtlas`
4. `RenderPreviewAovs`

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

## Extension Points

Future simulation sensor outputs should start at the output layer:

- Add another AOV backed by the current scene: extend `RasterOutputController`
  with a focused pass that reuses `RasterGpuScene`, `RasterSceneDescriptors`,
  and `RasterViewUniforms`.
- Add a sensor needing special framebuffer or image layout rules: follow the
  `TileAtlasPass` pattern and keep its resource state inside the pass.
- Add a sensor needing extra scene data: extend `RasterGpuScene` with a
  read-only access surface first, then consume that data from an output pass.

Hydra-specific token, render setting, and render buffer copy adaptation belongs
in `hdRobot/`; raster components should remain free of OpenUSD Hydra types.

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
