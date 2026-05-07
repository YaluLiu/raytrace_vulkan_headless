# File Map

This file is a navigation index for agents and maintainers. Use it after
`graphify-out/GRAPH_REPORT.md` to decide which files to read first for a
question or change. Treat it as a starting map, then verify definitions and
call sites with `rg`.

## Top-Level Structure

- `CMakeLists.txt`: Root build configuration, options, and subdirectory
  registration.
- `install.sh`: Main local build/run helper used by demo and hydra workflows.
- `headless/`: Core headless Vulkan ray tracing library and shaders.
- `hdRobot/`: OpenUSD Hydra render delegate plugin that bridges Hydra into the
  headless renderer.
- `demo/`: Standalone demo executable and USD loading helpers.
- `common/`: Shared model loader interfaces and OBJ loading implementation.
- `graphify-out/`: Generated knowledge graph and graph report.
- `docs/`: Tracked human/agent navigation and design documents.
- `docs/PLAN/materialx-rendering-plan.md`: Current OpenChessSet MaterialX
  support scope, approximations, and regression commands.

## Build And Test Routing

- `CMakeLists.txt`: Start here for project-wide options such as
  `ENABLE_HYDRA`, `ENABLE_GL_VK_CONVERSION`, `ENABLE_DLSS_RR`, and
  `DLSS_SDK_ROOT`.
- `headless/CMakeLists.txt`: Headless renderer library sources, shader
  compilation, DLSS SDK discovery, and renderer compile definitions.
- `hdRobot/CMakeLists.txt`: Hydra plugin library, USD dependencies, plugin
  metadata generation, and install layout.
- `demo/CMakeLists.txt`: Standalone demo target.
## Runtime Entry Points

- `demo/main.cpp`: Demo application entry and scene setup.
- `headless/ray_trace_app.cpp`: Headless app orchestration, context setup,
  resize forwarding, and default scene loading.
- `headless/ray_trace_app.hpp`: Headless app public surface and member state.
- `hdRobot/rendererPlugin.cpp`: Hydra plugin registration.
- `hdRobot/renderDelegate.cpp`: Hydra render delegate construction and render
  param ownership.
- `hdRobot/renderPass.cpp`: Hydra render pass entry; delegates frame execution
  to `HeadlessRenderBridge::RenderFrame`.
- `hdRobot/headlessRenderBridge.cpp`: Bridge between Hydra scene data and
  `HelloVulkan` frame execution.

## Headless Vulkan Renderer

- `headless/hello_vulkan.hpp`: Main `HelloVulkan` state, public controls,
  renderer resources, DLSS/offscreen fields, and helper declarations.
- `headless/hello_vulkan.cpp`: Core setup, frame rendering, resize handling,
  accumulation reset, render settings application, and DLSS/offscreen flow.
- `headless/hello_vulkan_hydra.cpp`: Hydra-facing scene synchronization,
  camera/light/material/mesh update paths, and accumulation invalidation.
- `headless/hello_vulkan_mesh.cpp`: Mesh upload, BLAS/TLAS updates, geometry
  conversion, and mesh-driven accumulation reset paths.
- `headless/hello_vulkan_material.cpp`: Material loading, material buffer
  updates, and model loading into the renderer.
- `headless/hello_vulkan_pipeline.cpp`: Ray tracing pipeline setup and shader
  binding table related logic.
- `headless/hello_vulkan_barriers.hpp`: Vulkan image layout/barrier helpers.
- `headless/headless_vk.cpp`: Headless Vulkan/EGL/OpenGL context support.
- `headless/gl_vkpp.hpp`: OpenGL/Vulkan interop helper declarations.

## DLSS And Offscreen Rendering

- `headless/dlss/dlss_rr.hpp`: DLSS-RR wrapper API, input structs, quality
  enums, and operational state surface.
- `headless/dlss/dlss_rr.cpp`: NGX runtime setup, optimal settings query,
  validation, resource wrapping, and DLSS evaluation.
- `headless/hello_vulkan.cpp`: Integration point for DLSS sizing,
  offscreen target refresh, jitter, evaluate calls, and fallback behavior.
- `headless/hello_vulkan.hpp`: DLSS/offscreen state and inline setter paths
  that refresh targets or reset accumulation.

## LiDAR Rendering

- `headless/hello_vulkan_lidar.cpp`: LiDAR camera updates, LiDAR ray tracing,
  point cloud rendering, composite descriptor setup, and related barriers.
- `headless/shaders/raytrace_lidar.rgen`: LiDAR ray generation shader.
- `headless/shaders/lidar_pointcloud.glsl`: LiDAR point cloud shader.
- `headless/shaders/lidar_composite.comp`: LiDAR composite compute shader.

## Hydra Delegate

- `hdRobot/renderDelegate.h` / `hdRobot/renderDelegate.cpp`: Hydra delegate
  ownership, supported primitives, render param setup, and resource access.
- `hdRobot/renderPass.h` / `hdRobot/renderPass.cpp`: Hydra render pass object
  and frame execution entry into the bridge.
- `hdRobot/headlessRenderBridge.h` / `hdRobot/headlessRenderBridge.cpp`:
  Converts Hydra frame state into `HelloVulkan` updates and render calls.
- `hdRobot/renderParam.h` / `hdRobot/renderParam.cpp`: Shared Hydra render
  parameter object and renderer bridge ownership.
- `hdRobot/renderSettings.h` / `hdRobot/renderSettings.cpp`: Render setting
  tokens, parsing, and application helpers.
- `hdRobot/tokens.h` / `hdRobot/tokens.cpp`: Hydra token definitions.
- `hdRobot/plugInfo.json`: Hydra plugin metadata template.

## Hydra Scene Primitives

- `hdRobot/mesh.h` / `hdRobot/mesh.cpp`: Mesh sync, topology/primvar analysis,
  tangent generation, material binding, visibility, transforms, and conversion
  into renderer mesh data.
- `hdRobot/material.h` / `hdRobot/material.cpp`: Material sync lifecycle,
  MaterialX parser invocation, texture registration, and dirty marking.
- `hdRobot/materialXParser.h` / `hdRobot/materialXParser.cpp`: MaterialX
  surface shader selection, standard surface/OpenPBR input rules, upstream
  texture traversal, texture binding metadata, and `HydraMaterial` field
  mapping.
- `hdRobot/camera.h` / `hdRobot/camera.cpp`: Camera sync and camera data
  conversion.
- `hdRobot/light.h` / `hdRobot/light.cpp`: Light sync and renderer light data.
- `hdRobot/points.h` / `hdRobot/points.cpp`: Points primitive support.
- `hdRobot/instancer.h` / `hdRobot/instancer.cpp`: Hydra instancer support.
- `hdRobot/sceneData.h` / `hdRobot/sceneData.cpp`: Shared scene data helpers.

## Render Buffers And Texture Export

- `hdRobot/renderBuffer.h` / `hdRobot/renderBuffer.cpp`: Hydra render buffer
  allocation, Hgi texture descriptor creation, GL interop, and texture format
  conversion.
- `hdRobot/renderTextureExport.h` / `hdRobot/renderTextureExport.cpp`: Render
  texture export surface, including Hydra material texture asset byte export.
- `hdRobot/utils.h` / `hdRobot/utils.cpp`: Utility functions shared by the
  Hydra plugin.

## Model And Scene Loading

- `common/ModelLoader.h`: Abstract model loading interface consumed by
  `HelloVulkan::loadModel`, including legacy texture filenames and in-memory
  encoded texture assets.
- `common/obj_loader.h` / `common/obj_loader.cpp`: OBJ loader implementation,
  vertices, indices, normals, texcoords, material assignment, and fallback
  normals.
- `demo/usd_loader.h` / `demo/usd_loader.cpp`: USD scene loading helper for the
  demo path.
- `demo/main.cpp`: Example scene composition and model loading calls.
- `headless/ray_trace_app.cpp`: Default headless scene loading.

## Shader Files

- `headless/shaders/host_device.h`: Shared host/device structs and constants.
- `headless/shaders/raycommon.glsl`: Common ray tracing shader helpers.
- `headless/shaders/raytrace.rgen`: Main ray generation shader.
- `headless/shaders/raytrace.rchit`: Main closest-hit shader.
- `headless/shaders/raytrace.rmiss`: Main miss shader.
- `headless/shaders/raytraceShadow.rmiss`: Shadow miss shader.
- `headless/shaders/raytrace.rint`: Intersection shader.
- `headless/shaders/raytrace2.rchit`: Additional closest-hit shader variant.
- `headless/shaders/wavefront.glsl`: Wavefront helper shader code.
- `headless/shaders/raytrace_lidar.rgen`: LiDAR ray generation shader.
- `headless/shaders/lidar_pointcloud.glsl`: LiDAR point cloud shader.
- `headless/shaders/lidar_composite.comp`: LiDAR composite compute shader.

## Question Routing

- Frame rendering or accumulation:
  `headless/hello_vulkan.cpp`, `headless/hello_vulkan.hpp`,
  `hdRobot/headlessRenderBridge.cpp`, then search for `RenderFrame`,
  `resetAccumulation`, and `applyRenderSettings`.
- Resize or render target size:
  `headless/hello_vulkan.cpp`, `headless/hello_vulkan.hpp`,
  `headless/ray_trace_app.cpp`, then search for `onResize`,
  `refreshOffscreenRenderTargetsIfNeeded`, `computeRenderSize`, and
  `initOrResize`.
- DLSS-RR:
  `headless/dlss/dlss_rr.cpp`, `headless/dlss/dlss_rr.hpp`,
  `headless/hello_vulkan.cpp`, then search for `queryOptimalSettings`,
  `createDlssRR`, `evaluate`, and `DLSS_RR`.
- LiDAR:
  `headless/hello_vulkan_lidar.cpp` and LiDAR shader files, then search for
  `renderLidarPointCloud`, `updateLidarCamera`, and
  `updateLidarCompositeDescriptorSet`.
- Hydra render flow:
  `hdRobot/renderPass.cpp`, `hdRobot/headlessRenderBridge.cpp`,
  `hdRobot/renderDelegate.cpp`, then search for `RenderFrame`,
  `_UpdateRenderScene`, and `Sync`.
- Hydra mesh sync:
  `hdRobot/mesh.cpp`, `hdRobot/mesh.h`, then search for `_CreateGiMeshes`,
  `_UpdateGeometry`, `_AnalyzePrimvars`, and tangent calculation helpers.
- Material or texture path issues:
  `hdRobot/material.cpp`, `headless/hello_vulkan_material.cpp`,
  `hdRobot/renderTextureExport.cpp`, `common/ModelLoader.h`,
  `common/obj_loader.cpp`, then search for `GetTexturePath`,
  `ExportRegisteredTextures`, `stbi_load_from_memory`, `loadMaterial`, and
  material buffer updates.
- PBR material struct or shader material layout:
  `common/data_loader.h`, `hdRobot/sceneData.h`, `hdRobot/sceneData.cpp`,
  `hdRobot/headlessRenderBridge.cpp`, `headless/shaders/host_device.h`,
  then search for `baseColorFactor`, `roughnessFactor`, and
  `normalTextureId`.
- MaterialX standard surface parsing:
  `hdRobot/materialXParser.cpp`, `hdRobot/material.cpp`,
  `hdRobot/renderTextureExport.cpp`,
  `hdRobot/sceneData.h`, then search for `ParseMaterialXNetwork`,
  `SelectSurfaceShaderCandidate`, `ResolveUpstreamTexture`,
  `MaterialInputRule`, `base_color`, `metalness`, `specular_roughness`, and
  `normalTextureId`.
- Texture usage or color space:
  `common/ModelLoader.h`, `hdRobot/sceneData.h`,
  `hdRobot/renderParam.cpp`, `hdRobot/renderTextureExport.cpp`,
  `headless/hello_vulkan_material.cpp`, then search for `TextureUsage`,
  `TextureColorSpaceForUsage`, `GetTextureAssets`, and
  `VK_FORMAT_R8G8B8A8_UNORM`.
- PBR shader lighting:
  `headless/shaders/raytrace.rchit`, `headless/shaders/wavefront.glsl`,
  `headless/shaders/raytrace.rgen`, then search for `samplePbrMaterial`,
  `computePbrDirectLighting`, `firstHitDiffuseValid`, and
  `firstHitSpecularPad`.
- Normal map support:
  `hdRobot/mesh.cpp`, `hdRobot/sceneData.h`, `hdRobot/sceneData.cpp`,
  `common/data_loader.h`, `headless/shaders/host_device.h`,
  `headless/shaders/raytrace.rchit`, then search for `tangent`,
  `bitangentSigns`, and `sampleNormalMap`.
- Advanced MaterialX inputs:
  `hdRobot/materialXParser.cpp`, `hdRobot/sceneData.h`,
  `hdRobot/sceneData.cpp`, `common/data_loader.h`,
  `headless/shaders/host_device.h`, `headless/shaders/raytrace.rchit`,
  `headless/shaders/wavefront.glsl`, then search for `transmissionFactor`,
  `transmissionColorFactor`, `subsurfaceFactor`, `subsurfaceTextureId`, and
  `computeSubsurfaceWrap`.
- Instancers or material subsets:
  `hdRobot/instancer.cpp`, `hdRobot/mesh.cpp`,
  `hdRobot/sceneData.cpp`, `hdRobot/headlessRenderBridge.cpp`, then search
  for `ComputeFlattenedTransforms`, `GetGeomSubsets`, `scene_mat_ids`,
  `materialIds`, and `tlasIds`.
- Render buffer or Hgi texture issues:
  `hdRobot/renderBuffer.cpp`, `hdRobot/renderBuffer.h`, then search for
  `createDesc`, `ConvertToHgiTexture`, `Allocate`, and `GetFormat`.
- Model loading:
  `common/obj_loader.cpp`, `common/obj_loader.h`, `common/ModelLoader.h`,
  `headless/hello_vulkan_material.cpp`, then search for `loadModel`,
  `loadVertices`, `loadIndices`, and `assignMaterialIndices`.
- Build or install failures:
  Root `CMakeLists.txt`, relevant subdirectory `CMakeLists.txt`,
  `install.sh`, and the failing command output.

## Graphify Anchors

The graph report currently identifies these high-connectivity anchors:

- `resetAccumulation()`
- `RenderFrame()`
- `loadModel()`
- `setupContext()`
- `refreshOffscreenRenderTargetsIfNeeded()`
- `createDesc()`
- `_CreateGiMeshes()`
- `queryOptimalSettings()`
- `evaluate()`
- `onResize()`

When a question is ambiguous, start from the matching anchor, inspect its
definition, then read direct callers/callees before widening to the full
community.
