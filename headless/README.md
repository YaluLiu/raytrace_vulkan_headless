# Headless Raster Renderer

The headless target now provides a compact Vulkan raster baseline for Hydra and
standalone rendering. It renders mesh color, normalized depth, primitive ID, and
instance ID AOVs through the offscreen framebuffer owned by `HelloVulkan`.

## Main Files

- `ray_trace_app.cpp` owns Vulkan context setup, resource creation, resize
  forwarding, and frame pass ordering.
- `hello_vulkan.cpp` owns renderer setup, render settings, frame uniforms,
  offscreen target allocation, and AOV export.
- `hello_vulkan_pipeline.cpp` owns descriptor layout updates, graphics pipeline
  creation, framebuffer creation, and raster draw submission.
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
3. `Rasterize`

The raster pass binds the shared scene descriptor set, then draws each visible
instance with `PushConstantRaster` data for model transform, object index, and
instance ID.

## AOVs

`HelloVulkan::GetAovTexture()` exposes the current offscreen Vulkan images for:

- `HeadlessAov::Color`
- `HeadlessAov::Depth`
- `HeadlessAov::PrimId`
- `HeadlessAov::InstanceId`

Hydra texture export and GL interop code consume these handles through
`hdRobot/renderTextureExport.cpp` and `hdRobot/glInteropCache.cpp`.
