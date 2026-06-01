# File Map

This file is a navigation index for agents and maintainers. Use it after
`graphify-out/GRAPH_REPORT.md` to decide which files to read first for a
question or change. Treat it as a starting map, then verify definitions and
call sites with `rg`.

## Top-Level Structure

- `CMakeLists.txt`: Root build configuration, package setup, and unconditional
  engine, `UsdRaySensor`, `UsdRaySensorImaging`, and `hdRobot` subdirectory
  registration. `ROBOT_ENGINE_SCHEMA_ONLY=ON` configures only the
  `UsdRaySensor` schema plugin and skips Vulkan/engine/Hydra setup.
- `install.sh`: Main local build/run helper used by Hydra workflows.
- `build_windows.bat`: Windows Hydra build helper; builds and installs
  `UsdRaySensor`, `UsdRaySensorImaging`, and `hdRobot`.
- `engine/`: Core Vulkan graphics library. Public API lives under
  `include/engine/`; cross-output core, scene, and runtime internal headers and
  implementations live together under `src/`; output-specific implementation
  and shaders live under `features/`; shared GLSL/C++ ABI includes remain under
  `shaders/common/`.
- `hdRobot/`: OpenUSD Hydra render delegate plugin that bridges Hydra into the
  engine renderer.
- `UsdRaySensor/`: Schema-only USD plugin for the custom
  `UsdGeomLidarSensor` and `UsdGeomHeightScanSensor` C++ schema APIs backed by
  stable `LidarSensor` and `HeightScanSensor` prim types; this target is the
  schema-only install component for non-Hydra consumers such as physics
  engines.
- `UsdRaySensorImaging/`: UsdImaging adapter plugin and shared Hydra sensor
  contract for exposing `LidarSensor` and `HeightScanSensor` prims as custom
  `lidarSensor` and `heightScanSensor` sprims.
- `graphify-out/`: Generated knowledge graph and graph report.
- `docs/`: Tracked human/agent navigation and design documents.

## Build And Test Routing

- `CMakeLists.txt`: Start here for project-wide package setup and target
  registration; use `-DROBOT_ENGINE_SCHEMA_ONLY=ON` for schema-only builds.
- `engine/CMakeLists.txt`: Engine renderer library sources, shader
  compilation, renderer compile definitions, and the focused
  `engine_height_scan_basis_test` CTest target.
- `hdRobot/CMakeLists.txt`: Hydra plugin library, USD dependencies, plugin
  metadata generation, and install layout.
- `UsdRaySensor/CMakeLists.txt`: Schema-only ray-sensor USD plugin target,
  plugin metadata generation, and schema resource install layout.
- `UsdRaySensorImaging/CMakeLists.txt`: UsdImaging adapter plugin target,
  shared Hydra sensor token export, metadata generation, and install layout.
- `build_windows.bat`: Windows route for building and installing the three USD
  plugin components into the configured USD plugin prefix.

## Runtime Entry Points

- `engine/src/runtime/session.cpp`: Engine session/runtime shell, engine-oriented
  Vulkan context setup, resize forwarding, command buffer begin/end/submit, and
  render resource lifecycle.
- `engine/include/engine/session.hpp`: `Session` public surface and renderer
  ownership.
- `engine/src/core/frame_executor.hpp` / `engine/src/core/frame_executor.cpp`:
  Source of truth for per-frame engine pass sequencing; preview AOV rendering
  is followed by LiDAR point overlay and then height scan overlay.
- `hdRobot/rendererPlugin.cpp`: Hydra plugin registration.
- `hdRobot/renderDelegate.cpp`: Hydra render delegate construction and render
  param ownership.
- `hdRobot/renderPass.cpp`: Hydra render pass entry; delegates frame execution
  to `HdRobotRenderBridge::RenderFrame`.
- `hdRobot/renderBridge.cpp`: Bridge between Hydra scene data and
  `Renderer` frame execution.
- `hdRobot/renderBridgeConversions.h` / `hdRobot/renderBridgeConversions.cpp`:
  Hydra-to-engine data conversion helpers used by the bridge for mesh geometry,
  materials, cameras, lights, LiDAR, height scan, visualization, and tile
  config objects.

## Engine Vulkan Renderer

- `engine/include/engine/renderer.hpp`: Main `Renderer` facade,
  public lifecycle controls, camera/tile/LiDAR/height-scan configuration,
  scene upload/update entry points, RT TLAS lifecycle and query entry points,
  and AOV export. Frame execution and descriptor/uniform resource steps are
  kept behind private core helpers.
- `engine/src/core/renderer.cpp`: Core facade setup, resize/lifecycle forwarding,
  AOV texture routing, and delegation into GPU scene, descriptor, view uniform,
  and output controller components.
- `engine/src/core/renderer_resources.hpp` /
  `engine/src/core/renderer_resources.cpp`: Private renderer resource
  helper layer used by `Session`, frame preparation, and texture-resource
  rebuilds to create/destroy descriptor sets, view uniforms, preview targets,
  output pipelines, light/object buffers, and RT resources without exposing
  these steps on the public `Renderer` API.
- `engine/src/core/renderer_internal.hpp`: Private bridge from
  `Renderer` to its PIMPL-owned GPU scene, descriptors, view uniforms,
  output controller, allocator, and debug utilities.
- `engine/include/engine/renderer_types.hpp`: Facade-facing data types such as
  `CameraSpec`, `MaterialUpdate`, and `TlasDescriptorInfo`.
- `engine/include/engine/output_config.hpp`: Public renderer output
  configuration snapshot that groups tile atlas, LiDAR, and height-scan inputs
  so callers can apply output state through one `Renderer::configureOutputs`
  call instead of mirroring per-pass setters across facade layers.
- `engine/include/engine/mesh_types.hpp`: Public engine mesh upload contract:
  CPU-side `MeshVertex`, `Material`, and `MeshGeometry`; mesh
  upload now passes geometry and material spans directly instead of using a
  combined upload DTO.
- `engine/include/engine/texture_asset.hpp`: Public engine texture asset
  contract: `TextureUsage`, `TextureColorSpace`, and encoded `TextureAsset`
  payloads passed from `hdRobot` to `Renderer`.
- `engine/src/debug/debug_readback.cpp`: Readback/debug helpers for object ID image
  downloads and offscreen color PNG output.
- `engine/include/engine/aov_texture.hpp`: Pure Vulkan engine AOV enum and texture
  descriptor returned by `Renderer::GetAovTexture`, including tile AOV
  atlas values exposed to Hydra without Hydra or OpenGL types. Display tile
  Hydra AOV tokens are bridge-side copy policies mapped onto the fixed tile
  atlas AOVs rather than separate engine outputs.
- `engine/src/scene/scene_types.hpp`: Shared draw input types used by
  `Renderer` and `PreviewPipeline`.
- `engine/src/scene/gpu_scene.hpp` / `engine/src/scene/gpu_scene.cpp`: Owner of mesh,
  material, instance, light, texture, object-description, and light GPU
  resources; uploads mesh geometry and material arrays through separate engine
  API parameters, provides read-only spans for output passes and facade methods
  for scene upload/update, and forwards RT acceleration-structure lifecycle
  and dirty updates into `RtScene`.
- `engine/src/scene/rt_scene.hpp` / `engine/src/scene/rt_scene.cpp`: RT
  acceleration-structure mirror for the engine scene; builds BLAS from uploaded
  mesh buffers, builds/updates TLAS from renderer instances, applies per-instance
  trace masks for normal geometry versus height-scan ground geometry, and
  exposes the TLAS descriptor primitive for RT LiDAR and height-scan passes.
- `engine/src/scene/scene_descriptors.hpp` /
  `engine/src/scene/scene_descriptors.cpp`: Owner of scene descriptor bindings,
  pool, layout, set, and descriptor updates for frame uniforms, object
  descriptions, lights, tile uniforms, and textures.
- `engine/src/scene/view_uniforms.hpp` / `engine/src/scene/view_uniforms.cpp`: Owner of
  main frame uniforms, tile frame uniforms, camera array, main camera clip
  range, and uniform buffer update logic.
- `engine/src/core/output_controller.hpp` /
  `engine/src/core/output_controller.cpp`: Output orchestration layer that owns
  the preview AOV pipeline, tile atlas pass, LiDAR point cloud pass, height scan
  pass, AOV texture query routing, and fan-out from the renderer output
  configuration snapshot into the concrete output passes.
- `engine/features/preview/preview_pipeline.hpp` / `engine/features/preview/preview_pipeline.cpp`:
  Main preview/offscreen AOV pipeline wrapper owning color/depth/id images,
  depth attachment, render pass, framebuffer, graphics pipelines, AOV texture
  export backing, dynamic frame-uniform descriptor binding, and draw command
  recording.
- `engine/include/engine/tile_config.hpp`: Pure engine tile output configuration contract,
  including enable flag, per-camera tile size, grid dimensions, and positive
  value normalization.
- `engine/features/tile/tile_aov_channel.hpp` / `engine/features/tile/tile_aov_channel.cpp`: Single-channel
  tile atlas output resource used by the multiview path; owns one atlas image,
  optional external-memory export, and layered image copies into row-major tile
  rects. `TileAtlasPass` holds separate color and depth channel atlases.
- `engine/features/tile/multiview_tile_targets.hpp` / `engine/features/tile/multiview_tile_targets.cpp`:
  Temporary layered color/depth-AOV/depth-attachment tile targets used by
  multiview tile batches before copying requested channels back into 2D atlases.
- `engine/features/tile/multiview_tile_pipeline.hpp` /
  `engine/features/tile/multiview_tile_pipeline.cpp`: Dedicated multiview tile
  render pass, graphics pipeline, dome background pipeline, and draw recording.
- `engine/features/tile/tile_atlas_pass.hpp` / `engine/features/tile/tile_atlas_pass.cpp`: Owner of tile
  atlas orchestration state, multiview capability state, dirty/export-valid
  state, layered tile targets, tile atlas images, and multiview tile pipeline.
- `engine/include/engine/lidar_types.hpp`: Public Hydra-free LiDAR sensor,
  visualization, point, per-sensor cloud, and frame cloud contract.
- `engine/features/lidar/lidar_scan.hpp` /
  `engine/features/lidar/lidar_scan.cpp`: Pure CPU LiDAR sample count, angle,
  basis, beam direction, and per-sensor layout helpers.
- `engine/features/lidar/lidar_point_cloud_pass.hpp` /
  `engine/features/lidar/lidar_point_cloud_pass.cpp`: LiDAR output
  coordinator owning sensors, visualization config, global point buffer,
  sensor metadata buffer, CPU readback, Vulkan RT point generation, and overlay
  pass orchestration.
- `engine/features/lidar/lidar_depth_pipeline.hpp` /
  `engine/features/lidar/lidar_depth_pipeline.cpp`: Legacy single-sensor
  angular range graphics pipeline. The active LiDAR point path now uses Vulkan RT
  ray queries through `lidar_points.comp`.
- `engine/features/lidar/lidar_point_generation_pipeline.hpp` /
  `engine/features/lidar/lidar_point_generation_pipeline.cpp`: Compute
  pipeline that binds the scene TLAS and traces one ray query per LiDAR beam
  into `LidarPointGpu` entries.
- `engine/features/lidar/lidar_point_overlay_pipeline.hpp` /
  `engine/features/lidar/lidar_point_overlay_pipeline.cpp`: Thin LiDAR
  compatibility wrapper around the shared point overlay pipeline.
- `engine/features/point_overlay/point_overlay_pipeline.hpp` /
  `engine/features/point_overlay/point_overlay_pipeline.cpp`: Shared preview
  point overlay graphics pipeline infrastructure for render pass, framebuffer,
  descriptor resources, push constants, and point-list draw binding. Sensor and
  point/sample layout details stay in overlay-specific vertex shaders.
- `engine/include/engine/height_scan_types.hpp`: Public Hydra-free height scan
  sensor pose/params, sample, per-sensor grid, and frame readback contract.
- `engine/features/height_scan/height_scan.hpp` /
  `engine/features/height_scan/height_scan.cpp`: Pure CPU height scan sample
  count, offset, forward-oriented basis, layout, and world-space origin helpers.
- `engine/tests/height_scan_basis_test.cpp`: Focused CTest coverage for height
  scan basis construction, including forward projection and degenerate fallback.
- `engine/features/height_scan/height_scan_generation_pipeline.hpp` /
  `engine/features/height_scan/height_scan_generation_pipeline.cpp`: Compute
  pipeline that binds the scene TLAS and traces one ground-mask ray query per
  height scan grid sample.
- `engine/features/height_scan/height_scan_pass.hpp` /
  `engine/features/height_scan/height_scan_pass.cpp`: Height scan output
  coordinator owning sensors, visualization config, GPU metadata, sample
  buffers, Vulkan RT generation, preview overlay orchestration, and CPU
  readback frames.
- `engine/src/scene/runtime_updates.cpp`: `Renderer` compatibility
  forwarding for runtime material and light list updates into `GpuScene`.
- `engine/src/scene/geometry_upload.cpp`: `Renderer` compatibility
  forwarding for instance and geometry updates into `GpuScene`.
- `engine/src/scene/scene_upload.cpp`: `Renderer` compatibility forwarding
  for mesh-source and texture upload into `GpuScene`, including
  `rebuildTextureResourcesAndSceneBindings` coordination across descriptors and
  output pipelines.
- `engine/src/core/renderer_resources.cpp`: Private `Renderer`
  resource orchestration for descriptor creation/update, frame uniform buffer
  capacity, preview target resize, render-resource creation/destruction, and
  output pipeline rebuilds.
- `engine/src/image_barriers.hpp`: Vulkan image layout/barrier helpers.
- `engine/src/runtime/headless_vk.cpp`: Headless Vulkan offline app context support.

## Offscreen Rendering

- `engine/include/engine/aov_texture.hpp`: Public engine AOV contract, currently
  `color`, `depth`, `primId`, `instanceId`, `tileColor`, and `tileDepth`.
- `engine/features/preview/preview_pipeline.hpp` / `engine/features/preview/preview_pipeline.cpp`: Offscreen
  target allocation, resize refresh, AOV texture export backing, direct
  color/depth/id target selection, and framebuffer rebuilds.
- `engine/src/core/output_controller.cpp`: `GetAovTexture` routing for standard
  preview AOVs and current tile atlas export images.
- `engine/features/tile/tile_aov_channel.hpp` / `engine/features/tile/tile_aov_channel.cpp`,
  `engine/features/tile/multiview_tile_targets.hpp` / `engine/features/tile/multiview_tile_targets.cpp`,
  `engine/features/tile/multiview_tile_pipeline.hpp` /
  `engine/features/tile/multiview_tile_pipeline.cpp`, and
  `engine/features/tile/tile_atlas_pass.hpp` /
  `engine/features/tile/tile_atlas_pass.cpp`: Per-channel tile atlas
  resources, layered multiview tile resources, layer-to-atlas copy,
  Hydra-exportable atlas color/depth sources, and local save suppression.
  `TileAtlasPass` intersects tile settings with the current Hydra tile AOV
  request mask before allocating and exporting channel atlases.

## Engine Baseline

- `engine/features/preview/shaders/mesh.vert`: Active mesh vertex shader.
- `engine/features/preview/shaders/mesh.frag`: Active mesh fragment shader for color,
  primitive ID, instance ID, depth AOV writes, sphere/simple light shading, and
  neutral-luminance DomeLight surface lighting from the renderer light buffer.
- `engine/features/tile/shaders/tile_multiview.vert` /
  `engine/features/tile/shaders/tile_multiview.frag`: Mesh shader variants for multiview
  tile rendering using `gl_ViewIndex` and `TileFrameUniforms`, including
  neutral-luminance DomeLight surface lighting.
- `engine/features/tile/shaders/dome_background_tile_multiview.vert` /
  `engine/features/tile/shaders/dome_background_tile_multiview.frag`: Dome background
  variants for multiview tile rendering.
- `engine/features/lidar/shaders/lidar_depth.vert` /
  `engine/features/lidar/shaders/lidar_depth.frag`: Legacy mesh variants that project
  scene geometry into a LiDAR angular range image.
- `engine/features/lidar/shaders/lidar_points.comp`: Active Vulkan RT LiDAR compute
  shader; performs one ray query against the scene TLAS per beam and writes the
  global LiDAR point buffer.
- `engine/features/height_scan/shaders/height_scan.comp`: Active Vulkan RT height scan
  compute shader; traces only TLAS instances carrying the ground trace mask and
  writes height scan sample buffers for CPU readback.
- `engine/features/lidar/shaders/lidar_overlay.vert` /
  `engine/features/lidar/shaders/lidar_overlay.frag`: Preview point overlay shaders for
  valid LiDAR hits.
- `engine/features/height_scan/shaders/height_scan_overlay.vert` /
  `engine/features/height_scan/shaders/height_scan_overlay.frag`: Preview point overlay
  shaders for valid height scan hit samples, drawn from generated GPU sample
  buffers after preview rendering.
- `engine/shaders/common/host_device.h`: Shared engine descriptor bindings,
  material structs, frame uniforms, tile multiview uniforms, LiDAR and height
  scan GPU structs, trace mask constants, and push constants.

## Hydra Delegate

- `hdRobot/renderDelegate.h` / `hdRobot/renderDelegate.cpp`: Hydra delegate
  ownership, supported primitives, render param setup, tile render setting
  descriptors including per-channel tile color/depth enable flags, LiDAR and
  height scan visualization render settings, AOV descriptor lookup through the
  shared bridge-side AOV spec, and resource access.
- `hdRobot/renderPass.h` / `hdRobot/renderPass.cpp`: Hydra render pass object
  and frame execution entry into the bridge.
- `hdRobot/renderBridge.h` / `hdRobot/renderBridge.cpp`:
  Converts Hydra frame state into `Renderer` updates and render calls,
  including backend scene snapshots for cameras, meshes, materials, lights,
  LiDAR and height scan sensors, the current frame's requested tile AOV channel
  mask, sorted sensor forwarding, visualization config forwarding,
  `hdRobot:traceRole = "ground"` routing into engine instance trace masks, and
  ordered AOV copy groups that copy fixed tile AOVs before display tile AOVs
  and other AOVs. GPU upload still happens here rather than in
  `CommitResources()`.
- `hdRobot/renderBridgeConversions.h` / `hdRobot/renderBridgeConversions.cpp`:
  Converts Hydra-side bridge structs into engine-facing camera specs,
  `Material`, light records, LiDAR and height scan sensor specs,
  visualization configs, and `TileAtlasConfig`.
- `hdRobot/renderParam.h` / `hdRobot/renderParam.cpp`: Shared Hydra render
  parameter object owning render settings, the texture registry, and the
  backend scene table. It no longer exposes global prim arrays; prim wrappers
  enqueue backend updates and `CommitResources()` drains them.
- `hdRobot/backendHandles.h`: Stable generation handles for backend mesh,
  material, light, camera, LiDAR sensor, and height scan sensor records.
- `hdRobot/slotVector.h`: Small handle-indexed slot vector used by the backend
  scene table; validates index, generation, and occupied state before access.
- `hdRobot/backendScene.h` / `hdRobot/backendScene.cpp`: CPU-side Hydra backend
  scene table, pending update queues, dirty mesh/material consumption, active
  camera/sensor/light snapshots, and renderer mesh/instance ID bookkeeping.
- `hdRobot/tokens.h` / `hdRobot/tokens.cpp`: Hydra token definitions,
  including tile render settings, LiDAR/height scan visualization settings,
  mesh `hdRobot:traceRole`, `ground`, and tile AOV tokens. Sensor parameter
  attribute tokens live in `UsdRaySensor/`; shared ray-sensor sprim type tokens
  and dirty bits live in `UsdRaySensorImaging/`.
- `hdRobot/aovBridgeSpec.h` / `hdRobot/aovBridgeSpec.cpp`: Single source of
  truth for Hydra AOV token mapping to engine AOVs, Hydra default AOV
  descriptors, tile AOV channel request masks, copy ordering, and fixed-size
  versus display tile copy policy.
- `hdRobot/plugInfo.json`: Hydra render delegate plugin metadata. The custom
  sensor USD schema metadata lives in `UsdRaySensor/`, and adapter metadata
  lives in `UsdRaySensorImaging/`.

## USD Sensor Recognition

- `UsdRaySensor/plugInfo.json`: Schema-only plugin metadata for generated
  `UsdGeomLidarSensor` and `UsdGeomHeightScanSensor`, while preserving
  authored prim type/schema identifiers `LidarSensor` and `HeightScanSensor`.
- `UsdRaySensor/schema.usda`: Code-generation source for the concrete
  typed `UsdGeomLidarSensor` and `UsdGeomHeightScanSensor` C++ APIs,
  including per-attribute docs for units, coordinate-space semantics,
  defaults, and runtime sanitization behavior.
- `UsdRaySensor/generatedSchema.usda`: Runtime schema resource containing
  generated `LidarSensor` and `HeightScanSensor` fallback attributes and
  mirrored sensor attribute docs.
- `UsdRaySensor/api.h`, `UsdRaySensor/lidarSensor.h` /
  `UsdRaySensor/lidarSensor.cpp`, `UsdRaySensor/heightScanSensor.h` /
  `UsdRaySensor/heightScanSensor.cpp`, and `UsdRaySensor/tokens.h` /
  `UsdRaySensor/tokens.cpp`: Generated sensor typed schema APIs, schema
  tokens, generated attribute getters, and custom typed scalar value getters.
  Hydra transport uses individual schema attribute tokens.

## USD Sensor Imaging

- `UsdRaySensorImaging/plugInfo.json`: UsdImaging adapter plugin metadata for
  `LidarSensorAdapter` and `HeightScanSensorAdapter`.
- `UsdRaySensorImaging/api.h`: Export macros for the imaging plugin.
- `UsdRaySensorImaging/hydraSensor.h` /
  `UsdRaySensorImaging/hydraSensor.cpp`: Shared Hydra-facing contract for the
  custom `lidarSensor` and `heightScanSensor` sprim type tokens plus sensor
  dirty bits; this is owned by the imaging plugin so schema-only consumers do
  not link against Hydra.
- `UsdRaySensorImaging/lidarSensorAdapter.h` /
  `UsdRaySensorImaging/lidarSensorAdapter.cpp`: UsdImaging adapter for USD
  `LidarSensor` prims through the `UsdGeomLidarSensor` C++ schema; inserts
  the custom `lidarSensor` sprim via the shared Hydra sensor contract,
  forwards individual sensor attributes through generated schema getters, and
  handles local/root/ancestor transform variability and dirtying.
- `UsdRaySensorImaging/heightScanSensorAdapter.h` /
  `UsdRaySensorImaging/heightScanSensorAdapter.cpp`: UsdImaging adapter for USD
  `HeightScanSensor` prims through the `UsdGeomHeightScanSensor` C++ schema;
  inserts the custom `heightScanSensor` sprim via the shared Hydra sensor
  contract, forwards individual sensor attributes through generated schema
  getters, and handles local/root/ancestor transform variability and dirtying.

## Hydra Scene Primitives

- `hdRobot/mesh.h` / `hdRobot/mesh.cpp`: Mesh wrapper with backend handle,
  topology/primvar analysis, tangent generation, material-handle binding,
  displayColor material updates, visibility, transforms,
  `hdRobot:traceRole = "ground"` ingestion, and local `HydraMesh` update
  command generation.
- `hdRobot/material.h` / `hdRobot/material.cpp`: Material sync lifecycle,
  MaterialX parser invocation, texture registration, backend material handle
  ownership, and material update command generation.
- `hdRobot/materialXParser.h` / `hdRobot/materialXParser.cpp`: USD Preview
  Surface plus MaterialX surface selection, `ND_surface` closure traversal,
  standard surface/OpenPBR and selected BSDF/EDF input rules, upstream texture
  and primvar-reader traversal, texture binding metadata, and `HydraMaterial`
  field mapping.
- `hdRobot/camera.h` / `hdRobot/camera.cpp`: Camera sync, shared
  `HdRobotCameraData` pose/projection data, and transform-to-camera-data
  conversion used by cameras and sensor sprims. Camera data is enqueued into
  the backend scene table. Sensor-specific params live in their sensor headers.
- `hdRobot/lidarSensor.h` / `hdRobot/lidarSensor.cpp`: Hydra sprim for custom
  USD `LidarSensor` prims; caches transform and individual sensor parameters
  from `HdSceneDelegate::Get(id, token)` with type diagnostics, leaves cached
  defaults intact for empty values, validates/clamps params, and enqueues
  active/inactive `HdRobotLidarSensorData` updates. The header owns
  `HdRobotLidarParams` and `HdRobotLidarSensorData`.
- `hdRobot/heightScanSensor.h` / `hdRobot/heightScanSensor.cpp`: Hydra sprim
  for custom USD `HeightScanSensor` prims; caches transform and individual
  sensor parameters from `HdSceneDelegate::Get(id, token)` with type
  diagnostics, leaves cached defaults intact for empty values,
  validates/clamps params, derives camera forward/up from the transform, and
  enqueues active/inactive `HdRobotHeightScanSensorData` updates. The header
  owns `HdRobotHeightScanParams` and `HdRobotHeightScanSensorData`.
- `hdRobot/light.h` / `hdRobot/light.cpp`: Light wrappers with backend handles;
  sync computes local renderer light data and enqueues backend light updates.
- `hdRobot/instancer.h` / `hdRobot/instancer.cpp`: Hydra instancer support.
- `hdRobot/sceneData.h` / `hdRobot/sceneData.cpp`: Shared scene data helpers.

## Render Buffers And Texture Export

- `hdRobot/renderBuffer.h` / `hdRobot/renderBuffer.cpp`: Hydra render buffer
  allocation, Hgi texture descriptor creation, GL interop, and texture format
  conversion; AOV-specific usage decisions are derived from
  `hdRobot/aovBridgeSpec.*`.
- `hdRobot/hydraTextureAssetExport.h` / `hdRobot/hydraTextureAssetExport.cpp`:
  Resolves registered Hydra material texture assets and exports encoded bytes
  for engine upload.
- `hdRobot/hydraAovCopy.h` / `hdRobot/hydraAovCopy.cpp`: Maps Hydra
  AOV copy requests to exported engine AOV textures and copies them into Hydra
  render buffers through OpenGL interop; fixed `tileColor` and `tileDepth`
  copies require exact atlas-size render buffers, while display tile AOV copy
  requests can use GL blit scaling.
- `hdRobot/glInteropCache.h` / `hdRobot/glInteropCache.cpp`: Hydra-owned
  Vulkan external-memory import cache for engine AOV textures exposed as
  source OpenGL textures, including per-export eviction used for one retry after
  stale import or copy failures.
- `hdRobot/utils.h` / `hdRobot/utils.cpp`: Utility functions shared by the
  Hydra plugin.

## Model And Scene Loading

- `engine/include/engine/mesh_types.hpp`: Mesh upload/update DTOs consumed by
  `Renderer::uploadMesh` and `Renderer::updateMeshGeometry`; the
  CPU-side vertex/material layout is checked against
  `engine/shaders/common/host_device.h`.
- `engine/include/engine/texture_asset.hpp`: Texture usage, color-space, and
  encoded-byte payload DTOs consumed by `Renderer::loadTextureAssets`.

## Shader Files

- `engine/shaders/common/host_device.h`: Shared host/device structs and constants.
  `FrameUniforms` carries one camera/light-count slot selected by dynamic
  uniform offset; `TileFrameUniforms` carries per-batch multiview tile cameras;
  `MeshDrawPushConstants` carries per-draw model/object/instance data for the
  graphics path.
- `engine/features/preview/shaders/mesh.vert`: Minimal mesh vertex shader for mesh
  positions, normals, vertex colors, texcoords, tangents, and per-instance
  transform/object metadata.
- `engine/features/preview/shaders/mesh.frag`: Minimal mesh fragment shader writing color,
  primId, instanceId, normalized depth AOV attachments, and sphere/simple light
  diffuse shading from the shared light buffer. DomeLight surface lighting is
  converted to neutral luminance so its intensity illuminates materials without
  tinting their base color.
- `engine/features/tile/shaders/tile_multiview.vert` /
  `engine/features/tile/shaders/tile_multiview.frag`: Multiview tile mesh shader variants
  that read camera arrays from `TileFrameUniforms`.
- `engine/features/tile/shaders/dome_background_tile_multiview.vert` /
  `engine/features/tile/shaders/dome_background_tile_multiview.frag`: Multiview tile dome
  background variants that reconstruct view directions per `gl_ViewIndex`.

## Question Routing

- Frame rendering:
  `engine/src/core/frame_executor.cpp`, `engine/src/core/renderer.cpp`,
  `engine/src/core/output_controller.cpp`, `engine/features/tile/tile_atlas_pass.cpp`,
  `engine/features/lidar/lidar_point_cloud_pass.cpp`, `hdRobot/renderBridge.cpp`,
  then search for `RenderFrame`, `recordFramePasses`,
  `recordPreviewAovs`, `recordTileAtlas`, `recordLidarPointClouds`,
  `recordLidarPointOverlay`, `recordFrameUniformUpdate`, and `updateScene`.
- Resize or render target size:
  `engine/src/core/renderer.cpp`, `engine/include/engine/renderer.hpp`,
  `engine/features/preview/preview_pipeline.cpp`, `engine/src/runtime/session.cpp`, then search
  for `onResize`, `resizeRenderTargets`, `recreateAovTargets`, `getRenderSize`, and
  `ensureSessionReady`.
- Offscreen AOV export:
  `engine/include/engine/aov_texture.hpp`, `engine/features/preview/preview_pipeline.cpp`,
  `engine/src/core/output_controller.cpp`, `engine/features/tile/tile_atlas_pass.cpp`,
  `engine/features/tile/tile_aov_channel.cpp`,
  `hdRobot/aovBridgeSpec.cpp`, `hdRobot/hydraAovCopy.cpp`, then search for
  `GetAovTexture`, `getAovTexture`, `setRequestedTileAovChannels`, `Aov`, and
  `CopyAovToRenderBuffer`.
- Graphics pipeline:
  `engine/features/preview/preview_pipeline.hpp`, `engine/features/preview/preview_pipeline.cpp`,
  `engine/src/core/renderer_resources.cpp`, `engine/features/preview/shaders/mesh.vert`,
  `engine/features/preview/shaders/mesh.frag`, then search for `PreviewPipeline`,
  `createGraphicsPipeline`, `createFramebuffer`, `recordPreviewAovs`, and
  `MeshDrawPushConstants`.
- Hydra render flow:
  `hdRobot/renderPass.cpp`, `hdRobot/renderBridge.cpp`,
  `hdRobot/renderDelegate.cpp`, then search for `RenderFrame`,
  `_Execute`, `ApplyPendingUpdates`, and `Sync`.
- Hydra camera and multi-camera flow:
  `hdRobot/camera.cpp`, `hdRobot/renderParam.h`,
  `hdRobot/renderBridge.cpp`, `engine/include/engine/renderer.hpp`,
  `engine/src/scene/view_uniforms.cpp`, and `engine/features/tile/tile_atlas_pass.cpp`, then
  search for `CreateCamera`,
  `EnqueueCameraUpdate`, `GetActiveCameraSnapshot`,
  `GetActiveLidarSensorSnapshot`, `GetActiveHeightScanSensorSnapshot`,
  `setCameras`, `setMainCamera`, `setTileConfig`, `setRequestedTileAovChannels`,
  `recordTileAovAtlas`, and `CameraSpec`.
- LiDAR point cloud generation or overlay:
  `engine/include/engine/lidar_types.hpp`,
  `engine/features/lidar/lidar_scan.cpp`,
  `engine/features/lidar/lidar_point_cloud_pass.cpp`,
  `engine/features/lidar/lidar_point_generation_pipeline.cpp`,
  `engine/features/lidar/lidar_point_overlay_pipeline.cpp`,
  `engine/src/scene/rt_scene.cpp`,
  `engine/features/lidar/shaders/lidar_points.comp`,
  `engine/features/lidar/shaders/lidar_overlay.vert`,
  `hdRobot/renderDelegate.cpp`, and `hdRobot/renderBridge.cpp`, then search
  for `LidarSensorSpec`, `LidarPointCloudPass`,
  `GenerateLidarPointClouds`, `OverlayLidarPointCloud`,
  `hdRobot:lidar:visualizeEnabled`, `readLidarPointCloudFrame`, and
  `BuildLidarScanLayout`.
  For Hydra-side LiDAR discovery, start with
  `UsdRaySensorImaging/lidarSensorAdapter.cpp`,
  `UsdRaySensorImaging/plugInfo.json`,
  `UsdRaySensor/plugInfo.json`,
  `UsdRaySensor/generatedSchema.usda`, `hdRobot/lidarSensor.cpp`,
  `hdRobot/renderDelegate.cpp`, and `hdRobot/renderBridge.cpp`, then search
  for `UsdGeomLidarSensor`, `schemaIdentifier`, `lidarSensor`,
  `CreateLidarSensor`, `EnqueueLidarSensorUpdate`, and `GetActiveLidarSensorSnapshot`.
- Height scan generation or overlay:
  `engine/include/engine/height_scan_types.hpp`,
  `engine/features/height_scan/height_scan.hpp`,
  `engine/features/height_scan/height_scan.cpp`,
  `engine/features/height_scan/height_scan_generation_pipeline.hpp`,
  `engine/features/height_scan/height_scan_generation_pipeline.cpp`,
  `engine/features/height_scan/height_scan_pass.hpp`,
  `engine/features/height_scan/height_scan_pass.cpp`,
  `engine/features/height_scan/shaders/height_scan.comp`,
  `engine/features/height_scan/shaders/height_scan_overlay.vert`,
  `engine/features/height_scan/shaders/height_scan_overlay.frag`,
  `engine/features/point_overlay/point_overlay_pipeline.cpp`,
  `engine/src/core/frame_executor.cpp`,
  `engine/src/core/output_controller.cpp`,
  `UsdRaySensorImaging/heightScanSensorAdapter.cpp`,
  `UsdRaySensorImaging/plugInfo.json`,
  `UsdRaySensor/plugInfo.json`,
  `UsdRaySensor/generatedSchema.usda`,
  `hdRobot/heightScanSensor.cpp`, `hdRobot/renderDelegate.cpp`,
  `hdRobot/renderBridge.cpp`, and
  `engine/src/scene/rt_scene.cpp`, then search for
  `HeightScanSensorSpec`, `HeightScanVisualizationConfig`,
  `HeightScanPass`, `GenerateHeightScans`, `OverlayHeightScans`,
  `BuildHeightScanBasis`, `camera.forward`, `hdRobot:heightScan:visualizeEnabled`, `hdRobot:traceRole`,
  `kTraceMaskGround`, `TRACE_MASK_GROUND`,
  `UsdGeomHeightScanSensor`, `heightScanSensor`, `CreateHeightScanSensor`,
  `EnqueueHeightScanSensorUpdate`,
  `GetActiveHeightScanSensorSnapshot`, `setHeightScanSensors`,
  `setHeightScanVisualizationConfig`, and `readHeightScanFrame`.
- Hydra mesh sync:
  `hdRobot/mesh.cpp`, `hdRobot/mesh.h`, `hdRobot/tokens.h`,
  `hdRobot/renderBridge.cpp`, and `engine/src/scene/scene_types.hpp`,
  then search for `_CreateGiMeshes`, `updateGeometry`, `_AnalyzePrimvars`,
  `traceRole`, `ground`, `traceMask`, and tangent calculation helpers.
- Material or texture path issues:
  `hdRobot/material.cpp`, `engine/src/scene/gpu_scene.cpp`,
  `hdRobot/hydraTextureAssetExport.cpp`, `engine/include/engine/texture_asset.hpp`, then search
  for `GetTexturePath`, `ExportRegisteredTextures`, `stbi_load_from_memory`,
  `loadMaterial`, and material buffer updates.
- PBR material struct or shader material layout:
  `engine/include/engine/mesh_types.hpp`, `hdRobot/sceneData.h`,
  `hdRobot/sceneData.cpp`, `hdRobot/renderBridge.cpp`,
  `engine/shaders/common/host_device.h`,
  then search for `baseColorFactor`, `emissionFactor`,
  `diffuseTextureId`, `roughnessFactor`, and `normalTextureId`.
- USD Preview/MaterialX surface parsing:
  `hdRobot/materialXParser.cpp`, `hdRobot/material.cpp`,
  `hdRobot/hydraTextureAssetExport.cpp`,
  `hdRobot/sceneData.h`, then search for `ParseMaterialXNetwork`,
  `SelectSurfaceShaderCandidate`, `ResolveUpstreamTexture`,
  `ResolveUpstreamPrimvar`, `MaterialInputRule`, `UsdPreviewSurface`,
  `base_color`, `metalness`, `specular_roughness`, and `normalTextureId`.
- Texture usage or color space:
  `engine/include/engine/texture_asset.hpp`, `hdRobot/sceneData.h`,
  `hdRobot/renderParam.cpp`, `hdRobot/hydraTextureAssetExport.cpp`,
  `engine/src/scene/gpu_scene.cpp`, then search for `TextureUsage`,
  `TextureColorSpaceForUsage`, `GetTextureAssets`, and
  `VK_FORMAT_R8G8B8A8_UNORM`.
- Mesh shader material sampling:
  `engine/features/preview/shaders/mesh.frag`, `engine/shaders/common/host_device.h`, then
  search for `sampleBaseColor`, `WaveFrontMaterial`, and
  `baseColorTextureId`.
- Engine light evaluation:
  `hdRobot/light.cpp`, `hdRobot/sceneData.cpp`,
  `hdRobot/renderBridge.cpp`, `engine/src/scene/runtime_updates.cpp`,
  `engine/src/scene/gpu_scene.cpp`, `engine/src/scene/scene_descriptors.cpp`,
  `engine/shaders/common/host_device.h`, `engine/features/preview/shaders/mesh.frag`, then search
  for `ToLight`, `updateLights`, `updateLightBuffer`, `eLights`,
  and `evaluateSphereLight`.
- Normal map data flow:
  `hdRobot/mesh.cpp`, `hdRobot/sceneData.h`, `hdRobot/sceneData.cpp`,
  `engine/include/engine/mesh_types.hpp`, `engine/shaders/common/host_device.h`,
  `engine/features/preview/shaders/mesh.vert`, then search for `tangent`,
  `bitangentSigns`, and `normalTextureId`.
- Advanced MaterialX inputs:
  `hdRobot/materialXParser.cpp`, `hdRobot/sceneData.h`,
  `hdRobot/sceneData.cpp`, `engine/include/engine/mesh_types.hpp`,
  `engine/shaders/common/host_device.h`, then search for `transmissionFactor`,
  `transmissionColorFactor`, `subsurfaceFactor`, `subsurfaceTextureId`, and
  the matching material parser fields.
- Instancers or material subsets:
  `hdRobot/instancer.cpp`, `hdRobot/mesh.cpp`,
  `hdRobot/sceneData.cpp`, `hdRobot/renderBridge.cpp`, then search
  for `ComputeFlattenedTransforms`, `GetGeomSubsets`, `scene_mat_ids`,
  `materialIds`, and `rendererInstanceIds`.
- Render buffer or Hgi texture issues:
  `hdRobot/renderBuffer.cpp`, `hdRobot/renderBuffer.h`, then search for
  `createDesc`, `ConvertToHgiTexture`, `Allocate`, and `GetFormat`.
- Mesh upload staging:
  `engine/include/engine/mesh_types.hpp`,
  `engine/src/scene/gpu_scene.cpp`,
  `engine/src/scene/scene_upload.cpp`, and
  `hdRobot/renderBridgeConversions.cpp`,
  then search for `ConvertHydraMeshToGeometry`, `ToMaterial`,
  `uploadMesh`, `loadVertices`, `loadIndices`, and `assignMaterialIndices`.
- Build or install failures:
  Root `CMakeLists.txt`, relevant subdirectory `CMakeLists.txt`,
  `install.sh`, and the failing command output.

## Graphify Anchors

The graph report currently identifies these high-connectivity anchors:

- `GetOrImportSourceGlTexture()`
- `_CreateGiMeshes()`
- `ensureResources()`
- `createDesc()`
- `RenderFrame()`
- `destroy()`
- `ensureSessionReady()`
- `updateScene()`
- `Sync()`
- `_ProcessPrimvar()`

When a question is ambiguous, start from the matching anchor, inspect its
definition, then read direct callers/callees before widening to the full
community.
