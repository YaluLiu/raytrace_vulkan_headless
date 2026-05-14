# Headless Raster Renderer

The headless target now provides a compact Vulkan raster baseline for Hydra and
standalone rendering. It renders mesh color, normalized depth, primitive ID, and
instance ID AOVs through the offscreen framebuffer owned by `RasterPipeline`.

## Main Files

- `ray_trace_app.cpp` owns Vulkan context setup, resource creation, resize
  forwarding, and frame pass ordering.
- `hello_vulkan.cpp` owns renderer setup, render settings, frame uniforms,
  resize/lifecycle forwarding, and AOV export wrappers.
- `raster_pipeline.hpp` and `raster_pipeline.cpp` own the main raster
  offscreen AOV targets, graphics pipeline, framebuffer, AOV export backing,
  and draw command recording.
- `hello_vulkan_pipeline.cpp` keeps descriptor layout updates and compatibility
  wrappers that route the main raster pass into `RasterPipeline`.
- `tile_config.hpp`, `tile_atlas.hpp`, `tile_atlas.cpp`, and
  `hello_vulkan_tile.cpp` implement local-only multi-camera tile atlas output.
  Tile output uses an independent scratch raster pipeline plus independent
  color/depth atlas images, then writes stable files under `output/`.
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
index, and instance ID.

When tile output is enabled, headless renders cameras from the current camera
array serially into a per-tile scratch pipeline, copies color and float depth
into row-major atlas positions, restores the main camera state, and saves:

- `output/tile_color_atlas.png`
- `output/tile_depth_atlas.f32`
- `output/tile_depth_atlas.json`

The atlas size is controlled only by tile configuration:
`tileCameraWidth * gridColumns` by `tileCameraHeight * gridRows`.

## AOVs

`HelloVulkan::GetAovTexture()` exposes the current offscreen Vulkan images for:

- `HeadlessAov::Color`
- `HeadlessAov::Depth`
- `HeadlessAov::PrimId`
- `HeadlessAov::InstanceId`
- `HeadlessAov::TileColor` (token reserved; not exported to Hydra in this stage)
- `HeadlessAov::TileDepth` (token reserved; not exported to Hydra in this stage)
- `HeadlessAov::TileColorDisplay` (token reserved; not exported to Hydra in this stage)
- `HeadlessAov::TileDepthDisplay` (token reserved; not exported to Hydra in this stage)

Hydra texture export and GL interop code consume these handles through
`hdRobot/renderTextureExport.cpp` and `hdRobot/glInteropCache.cpp`. Tile atlas
data is currently local file output only.
