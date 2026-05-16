# Headless Raster Renderer

The headless target now provides a compact Vulkan raster baseline for Hydra and
standalone rendering. It renders mesh color, normalized depth, primitive ID, and
instance ID AOVs through the offscreen framebuffer owned by `RasterPipeline`.

## Main Files

- `ray_trace_app.cpp` owns Vulkan context setup, resource creation, resize
  forwarding, and frame pass ordering.
- `hello_vulkan.cpp` owns renderer setup, render settings, dynamic frame
  uniform slots, resize/lifecycle forwarding, and AOV export wrappers.
- `raster_pipeline.hpp` and `raster_pipeline.cpp` own the main raster
  offscreen AOV targets, graphics pipeline, framebuffer, AOV export backing,
  and draw command recording.
- `hello_vulkan_pipeline.cpp` keeps descriptor layout updates and compatibility
  wrappers that route the main raster pass into `RasterPipeline`.
- `tile_config.hpp`, `tile_atlas.hpp`, `tile_atlas.cpp`, and
  `hello_vulkan_tile.cpp` implement multi-camera tile atlas rendering. Tile
  output uses atlas-sized framebuffer attachments and per-tile viewport/scissor
  rectangles, then exposes atlas color/depth through Hydra AOV export.
- `hello_vulkan_mesh.cpp` owns mesh upload, geometry refresh, and per-instance
  transform or visibility updates.
- `hello_vulkan_hydra.cpp` applies Hydra camera, light, material, and scene
  updates before each frame.
- `shaders/raster.vert` and `shaders/raster.frag` are the active shader entry
  points for mesh rendering.
- `shaders/host_device.h` defines the shared host/device buffer and push
  constant layout.

## Frame Flow

`RayTraceApp::render()` executes three ordered passes:

1. `UpdateUniforms`
2. `UpdateLights`
3. `Rasterize`, which first runs `HelloVulkan::renderTileAtlas()` when tile
   output is enabled, then runs `HelloVulkan::rasterize()` for the main output

The raster pipeline binds the shared scene descriptor set, then draws each
visible instance with `PushConstantRaster` data for model transform, object
index, and instance ID. The frame uniform binding is dynamic: slot 0 is used
for the main output, and tile cameras use slots starting at 1.

When tile output is enabled, headless renders cameras from the current camera
array into row-major atlas rectangles using per-camera frame uniform slots. It
does not save tile color or tile depth files locally; Hydra consumes the atlas
through tile AOV render buffers.

The atlas size is controlled only by tile configuration:
`tileCameraWidth * gridColumns` by `tileCameraHeight * gridRows`.

## AOVs

`HelloVulkan::GetAovTexture()` exposes the current offscreen Vulkan images for:

- `HeadlessAov::Color`
- `HeadlessAov::Depth`
- `HeadlessAov::PrimId`
- `HeadlessAov::InstanceId`
- `HeadlessAov::TileColor`
- `HeadlessAov::TileDepth`
- `HeadlessAov::TileDisplayColor`
- `HeadlessAov::TileDisplayDepth`

Hydra texture export and GL interop code consume these handles through
`hdRobot/renderTextureExport.cpp` and `hdRobot/glInteropCache.cpp`. Fixed tile
AOVs use exact atlas-sized render buffers, while display tile AOVs can be
scaled to the viewport-sized render buffer.
