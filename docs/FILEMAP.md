# File Map

This file is a navigation index for agents and maintainers. Use it after
`graphify-out/GRAPH_REPORT.md` to decide which files to read first for a
question or change. Treat it as a starting map, then verify definitions and
call sites with `rg`.

## Top-Level Structure

- `CMakeLists.txt`: Root build configuration, package setup, and unconditional
  headless plus `hdRobot` subdirectory registration.
- `install.sh`: Main local build/run helper used by Hydra workflows.
- `headless/`: Core headless Vulkan rasterization library and shaders.
- `hdRobot/`: OpenUSD Hydra render delegate plugin that bridges Hydra into the
  headless renderer.
- `common/`: Shared model loader interfaces and OBJ loading implementation.
- `graphify-out/`: Generated knowledge graph and graph report.
- `docs/`: Tracked human/agent navigation and design documents.

## Build And Test Routing

- `CMakeLists.txt`: Start here for project-wide package setup and target
  registration.
- `headless/CMakeLists.txt`: Headless renderer library sources, shader
  compilation, and renderer compile definitions.
- `hdRobot/CMakeLists.txt`: Hydra plugin library, USD dependencies, plugin
  metadata generation, and install layout.

## Runtime Entry Points

- `headless/ray_trace_app.cpp`: Headless app orchestration, raster-oriented
  Vulkan context setup, resize forwarding, frame pass sequencing, and render
  resource lifecycle.
- `headless/ray_trace_app.hpp`: Headless app public surface and member state.
- `hdRobot/rendererPlugin.cpp`: Hydra plugin registration.
- `hdRobot/renderDelegate.cpp`: Hydra render delegate construction and render
  param ownership.
- `hdRobot/renderPass.cpp`: Hydra render pass entry; delegates frame execution
  to `HeadlessRenderBridge::RenderFrame`.
- `hdRobot/headlessRenderBridge.cpp`: Bridge between Hydra scene data and
  `HelloVulkan` frame execution.

Naming note: `RayTraceApp` and `ray_trace_app.*` are legacy app-layer names kept
for this branch; their current implementation is raster-oriented. Rename them in
a separate cleanup if the public surface can absorb the churn.

## Headless Vulkan Renderer

- `headless/hello_vulkan.hpp`: Main `HelloVulkan` renderer facade, public
  controls, headless camera array storage, scene buffers, descriptor state,
  compatibility wrappers, and `RasterPipeline` ownership.
- `headless/hello_vulkan.cpp`: Core setup, camera array ingestion, dynamic
  frame uniform slot allocation, explicit per-camera uniform updates, resize
  forwarding, AOV texture wrapper, and renderer resource lifecycle.
- `headless/aov_texture.hpp`: Pure Vulkan headless AOV enum and texture
  descriptor returned by `HelloVulkan::GetAovTexture`, including reserved
  tile AOV enum values, without Hydra or OpenGL types.
- `headless/raster_scene_types.hpp`: Shared draw input types used by
  `HelloVulkan` and `RasterPipeline`.
- `headless/raster_pipeline.hpp` / `headless/raster_pipeline.cpp`: Main raster
  pipeline wrapper owning offscreen AOV images, depth attachment, raster render
  pass, framebuffer, graphics pipelines, AOV texture export backing, dynamic
  frame-uniform descriptor binding, and draw command recording.
- `headless/tile_config.hpp`: Pure headless tile output configuration contract,
  including enable flag, per-camera tile size, grid dimensions, and positive
  value normalization.
- `headless/tile_atlas.hpp` / `headless/tile_atlas.cpp`: Independent local tile
  atlas color/depth images plus GPU copy helpers that place per-camera scratch
  renders into row-major atlas tiles without touching main AOV images.
- `headless/hello_vulkan_tile.cpp`: `HelloVulkan` tile orchestration, including
  per-camera scratch raster passes using per-camera frame uniform slots, atlas
  readback, and stable local `output/tile_*` file writes.
- `headless/hello_vulkan_hydra.cpp`: Hydra-facing scene synchronization,
  light/material update paths, texture export allocation, and light buffer
  uploads.
- `headless/hello_vulkan_mesh.cpp`: Mesh upload, raster instance
  transform/visibility updates, and geometry buffer refresh.
- `headless/hello_vulkan_material.cpp`: Material loading, material buffer
  updates, and model loading into the renderer.
- `headless/hello_vulkan_pipeline.cpp`: Shared scene descriptor set updates
  plus compatibility wrappers that delegate the main raster pass to
  `RasterPipeline`.
- `headless/hello_vulkan_barriers.hpp`: Vulkan image layout/barrier helpers.
- `headless/headless_vk.cpp`: Headless Vulkan offline app context support.

## Offscreen Rendering

- `headless/aov_texture.hpp`: Public headless AOV contract, currently
  `color`, `depth`, `primId`, `instanceId`, and reserved tile AOV tokens.
- `headless/raster_pipeline.hpp` / `headless/raster_pipeline.cpp`: Offscreen
  target allocation, resize refresh, AOV texture export backing, direct
  color/depth/id target selection, and framebuffer rebuilds.
- `headless/hello_vulkan.cpp`: Public compatibility wrappers for resize and
  `GetAovTexture`.
- `headless/tile_atlas.hpp` / `headless/tile_atlas.cpp` and
  `headless/hello_vulkan_tile.cpp`: Local-only tile atlas resources and save
  path; current tile AOV tokens remain reserved for future Hydra return work.

## Raster Baseline

- `headless/shaders/raster.vert`: Active mesh vertex shader.
- `headless/shaders/raster.frag`: Active mesh fragment shader for color,
  primitive ID, instance ID, depth AOV writes, and sphere/simple light shading
  from the renderer light buffer. DomeLight is parsed but not evaluated here.
- `headless/shaders/host_device.h`: Shared raster descriptor bindings,
  material structs, frame uniforms, and push constants.

## Hydra Delegate

- `hdRobot/renderDelegate.h` / `hdRobot/renderDelegate.cpp`: Hydra delegate
  ownership, supported primitives, render param setup, tile render setting
  descriptors, AOV descriptors, and resource access.
- `hdRobot/renderPass.h` / `hdRobot/renderPass.cpp`: Hydra render pass object
  and frame execution entry into the bridge.
- `hdRobot/headlessRenderBridge.h` / `hdRobot/headlessRenderBridge.cpp`:
  Converts Hydra frame state into `HelloVulkan` updates and render calls,
  including the all-camera snapshot passed to headless.
- `hdRobot/renderParam.h` / `hdRobot/renderParam.cpp`: Shared Hydra render
  parameter object, camera array, tile config, scene dirty flags, texture
  registry, and renderer bridge ownership.
- `hdRobot/tokens.h` / `hdRobot/tokens.cpp`: Hydra token definitions,
  including tile render settings and tile AOV tokens.
- `hdRobot/plugInfo.json`: Hydra plugin metadata template.

## Hydra Scene Primitives

- `hdRobot/mesh.h` / `hdRobot/mesh.cpp`: Mesh sync, topology/primvar analysis,
  tangent generation, material binding, visibility, transforms, and conversion
  into renderer mesh data.
- `hdRobot/material.h` / `hdRobot/material.cpp`: Material sync lifecycle,
  MaterialX parser invocation, texture registration, and dirty marking.
- `hdRobot/materialXParser.h` / `hdRobot/materialXParser.cpp`: USD Preview
  Surface plus MaterialX surface selection, `ND_surface` closure traversal,
  standard surface/OpenPBR and selected BSDF/EDF input rules, upstream texture
  and primvar-reader traversal, texture binding metadata, and `HydraMaterial`
  field mapping.
- `hdRobot/camera.h` / `hdRobot/camera.cpp`: Camera sync, camera data
  conversion, and registration into `HdRobotRenderParam::v_camera`.
- `hdRobot/light.h` / `hdRobot/light.cpp`: Light sync and renderer light data.
- `hdRobot/points.h` / `hdRobot/points.cpp`: HdPoints sync surface; the
  minimal raster path currently does not draw points.
- `hdRobot/instancer.h` / `hdRobot/instancer.cpp`: Hydra instancer support.
- `hdRobot/sceneData.h` / `hdRobot/sceneData.cpp`: Shared scene data helpers.

## Render Buffers And Texture Export

- `hdRobot/renderBuffer.h` / `hdRobot/renderBuffer.cpp`: Hydra render buffer
  allocation, Hgi texture descriptor creation, GL interop, and texture format
  conversion.
- `hdRobot/renderTextureExport.h` / `hdRobot/renderTextureExport.cpp`: Render
  texture export surface, including Hydra material texture asset byte export
  and debug AOV token to headless texture mapping.
- `hdRobot/glInteropCache.h` / `hdRobot/glInteropCache.cpp`: Hydra-owned
  Vulkan external-memory import cache for headless AOV textures exposed as
  source OpenGL textures.
- `hdRobot/utils.h` / `hdRobot/utils.cpp`: Utility functions shared by the
  Hydra plugin.

## Model And Scene Loading

- `common/ModelLoader.h`: Abstract model loading interface consumed by
  `HelloVulkan::loadModel`, including legacy texture filenames and in-memory
  encoded texture assets.
- `common/obj_loader.h` / `common/obj_loader.cpp`: OBJ loader implementation,
  vertices, indices, normals, texcoords, material assignment, and fallback
  normals.

## Shader Files

- `headless/shaders/host_device.h`: Shared host/device structs and constants.
  `FrameUniforms` carries one camera/light-count slot selected by dynamic
  uniform offset; `PushConstantRaster` carries per-draw model/object/instance
  data for the raster path.
- `headless/shaders/raster.vert`: Minimal raster vertex shader for mesh
  positions, normals, vertex colors, texcoords, tangents, and per-instance
  transform/object metadata.
- `headless/shaders/raster.frag`: Minimal raster fragment shader writing color,
  primId, instanceId, normalized depth AOV attachments, and sphere/simple light
  diffuse shading from the shared light buffer.

## Question Routing

- Frame rendering:
  `headless/hello_vulkan.cpp`, `headless/hello_vulkan.hpp`,
  `headless/hello_vulkan_tile.cpp`, `hdRobot/headlessRenderBridge.cpp`, then
  search for `RenderFrame`, `rasterize`, `renderTileAtlas`,
  `updateUniformBuffer`, and `updateScene`.
- Resize or render target size:
  `headless/hello_vulkan.cpp`, `headless/hello_vulkan.hpp`,
  `headless/raster_pipeline.cpp`, `headless/ray_trace_app.cpp`, then search
  for `onResize`, `createOffscreenRender`, `getRenderSize`, and
  `ensureHeadlessReady`.
- Offscreen AOV export:
  `headless/aov_texture.hpp`, `headless/raster_pipeline.cpp`,
  `headless/hello_vulkan.cpp`, `hdRobot/renderTextureExport.cpp`, then search
  for `GetAovTexture`, `getAovTexture`, `HeadlessAov`, and
  `ExportRenderTexture`.
- Raster pipeline:
  `headless/raster_pipeline.hpp`, `headless/raster_pipeline.cpp`,
  `headless/hello_vulkan_pipeline.cpp`, `headless/shaders/raster.vert`,
  `headless/shaders/raster.frag`, then search for `RasterPipeline`,
  `createGraphicsPipeline`, `createFramebuffer`, `rasterize`, and
  `PushConstantRaster`.
- Hydra render flow:
  `hdRobot/renderPass.cpp`, `hdRobot/headlessRenderBridge.cpp`,
  `hdRobot/renderDelegate.cpp`, then search for `RenderFrame`,
  `_UpdateRenderScene`, and `Sync`.
- Hydra camera and multi-camera flow:
  `hdRobot/camera.cpp`, `hdRobot/renderParam.h`,
  `hdRobot/headlessRenderBridge.cpp`, `headless/hello_vulkan.hpp`, and
  `headless/hello_vulkan.cpp`, then search for `v_camera`,
  `GetCamerasSnapshot`, `setCameras`, `setMainCamera`, `setTileConfig`,
  `renderTileAtlas`, and `HeadlessCameraData`.
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
  then search for `baseColorFactor`, `emissionFactor`,
  `diffuseTextureId`, `roughnessFactor`, and `normalTextureId`.
- USD Preview/MaterialX surface parsing:
  `hdRobot/materialXParser.cpp`, `hdRobot/material.cpp`,
  `hdRobot/renderTextureExport.cpp`,
  `hdRobot/sceneData.h`, then search for `ParseMaterialXNetwork`,
  `SelectSurfaceShaderCandidate`, `ResolveUpstreamTexture`,
  `ResolveUpstreamPrimvar`, `MaterialInputRule`, `UsdPreviewSurface`,
  `base_color`, `metalness`, `specular_roughness`, and `normalTextureId`.
- Texture usage or color space:
  `common/ModelLoader.h`, `hdRobot/sceneData.h`,
  `hdRobot/renderParam.cpp`, `hdRobot/renderTextureExport.cpp`,
  `headless/hello_vulkan_material.cpp`, then search for `TextureUsage`,
  `TextureColorSpaceForUsage`, `GetTextureAssets`, and
  `VK_FORMAT_R8G8B8A8_UNORM`.
- Raster shader material sampling:
  `headless/shaders/raster.frag`, `headless/shaders/host_device.h`, then
  search for `sampleBaseColor`, `WaveFrontMaterial`, and
  `baseColorTextureId`.
- Raster light evaluation:
  `hdRobot/light.cpp`, `hdRobot/sceneData.cpp`,
  `hdRobot/headlessRenderBridge.cpp`, `headless/hello_vulkan_hydra.cpp`,
  `headless/hello_vulkan_pipeline.cpp`, `headless/shaders/host_device.h`,
  `headless/shaders/raster.frag`, then search for `HydraLight::toLight`,
  `updateLights`, `updateLightBuffer`, `eLights`, and `evaluateSphereLight`.
- Normal map data flow:
  `hdRobot/mesh.cpp`, `hdRobot/sceneData.h`, `hdRobot/sceneData.cpp`,
  `common/data_loader.h`, `headless/shaders/host_device.h`,
  `headless/shaders/raster.vert`, then search for `tangent`,
  `bitangentSigns`, and `normalTextureId`.
- Advanced MaterialX inputs:
  `hdRobot/materialXParser.cpp`, `hdRobot/sceneData.h`,
  `hdRobot/sceneData.cpp`, `common/data_loader.h`,
  `headless/shaders/host_device.h`, then search for `transmissionFactor`,
  `transmissionColorFactor`, `subsurfaceFactor`, `subsurfaceTextureId`, and
  the matching material parser fields.
- Instancers or material subsets:
  `hdRobot/instancer.cpp`, `hdRobot/mesh.cpp`,
  `hdRobot/sceneData.cpp`, `hdRobot/headlessRenderBridge.cpp`, then search
  for `ComputeFlattenedTransforms`, `GetGeomSubsets`, `scene_mat_ids`,
  `materialIds`, and `rendererInstanceIds`.
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

- `GetOrImportSourceGlTexture()`
- `_CreateGiMeshes()`
- `createDesc()`
- `RenderFrame()`
- `setupContext()`
- `createOffscreenRender()`
- `updateScene()`
- `Sync()`
- `_ProcessPrimvar()`
- `setup()`

When a question is ambiguous, start from the matching anchor, inspect its
definition, then read direct callers/callees before widening to the full
community.
