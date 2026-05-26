# File Map

This file is a navigation index for agents and maintainers. Use it after
`graphify-out/GRAPH_REPORT.md` to decide which files to read first for a
question or change. Treat it as a starting map, then verify definitions and
call sites with `rg`.

## Top-Level Structure

- `CMakeLists.txt`: Root build configuration, package setup, and unconditional
  raster, `hdRobotLidarUsd`, and `hdRobot` subdirectory registration.
- `install.sh`: Main local build/run helper used by Hydra workflows.
- `raster/`: Core Vulkan rasterization library, split into public
  `include/raster/`, internal `private/`, implementation `src/`, and shader
  source `shaders/` trees.
- `hdRobot/`: OpenUSD Hydra render delegate plugin that bridges Hydra into the
  raster renderer.
- `hdRobotLidarUsd/`: Standalone USD recognition plugin for the custom
  `LidarSensor` and `HeightScanSensor` schemas and UsdImaging adapters; it
  inserts the `lidarSensor` and `heightScanSensor` Hydra sprim types without
  depending on the `hdRobot` target.
- `common/`: Shared model loader interfaces and OBJ loading implementation.
- `graphify-out/`: Generated knowledge graph and graph report.
- `docs/`: Tracked human/agent navigation and design documents.
- `docs/ray_trace_lidar.md`: Chinese implementation plan for the Vulkan
  RT/ray-query based LiDAR path and related subtask ownership notes.

## Build And Test Routing

- `CMakeLists.txt`: Start here for project-wide package setup and target
  registration.
- `raster/CMakeLists.txt`: Raster renderer library sources, shader
  compilation, and renderer compile definitions.
- `hdRobot/CMakeLists.txt`: Hydra plugin library, USD dependencies, plugin
  metadata generation, and install layout.
- `hdRobotLidarUsd/CMakeLists.txt`: Standalone LiDAR USD schema/adapter plugin
  target, plugin metadata generation, and schema resource install layout.

## Runtime Entry Points

- `raster/src/runtime/raster_session.cpp`: Raster session/runtime shell, raster-oriented
  Vulkan context setup, resize forwarding, command buffer begin/end/submit, and
  render resource lifecycle.
- `raster/include/raster/raster_session.hpp`: `RasterSession` public surface and renderer
  ownership.
- `raster/private/core/raster_frame_executor.hpp` / `raster/src/core/raster_frame_executor.cpp`:
  Source of truth for per-frame raster pass sequencing; preview AOV rendering
  is followed by LiDAR point overlay and then height scan overlay.
- `hdRobot/rendererPlugin.cpp`: Hydra plugin registration.
- `hdRobot/renderDelegate.cpp`: Hydra render delegate construction and render
  param ownership.
- `hdRobot/renderPass.cpp`: Hydra render pass entry; delegates frame execution
  to `HdRobotRasterBridge::RenderRasterFrame`.
- `hdRobot/rasterBridge.cpp`: Bridge between Hydra scene data and
  `RasterRenderer` frame execution.

## Raster Vulkan Renderer

- `raster/include/raster/raster_renderer.hpp`: Main `RasterRenderer` facade, public controls,
  camera/tile/LiDAR/height-scan configuration, scene upload/update entry
  points, RT TLAS lifecycle and query entry points, AOV export, and temporary
  internal methods used by the frame executor while internals are componentized.
- `raster/src/core/raster_renderer.cpp`: Core facade setup, resize/lifecycle forwarding,
  AOV texture routing, and delegation into GPU scene, descriptor, view uniform,
  and output controller components.
- `raster/include/raster/raster_renderer_types.hpp`: Facade-facing data types such as
  `RasterCameraSpec`, `RasterMaterialUpdate`, and `RasterTlasDescriptorInfo`.
- `raster/src/debug/raster_debug_readback.cpp`: Readback/debug helpers for object ID image
  downloads and offscreen color PNG output.
- `raster/include/raster/aov_texture.hpp`: Pure Vulkan raster AOV enum and texture
  descriptor returned by `RasterRenderer::GetAovTexture`, including tile AOV
  values exposed to Hydra without Hydra or OpenGL types.
- `raster/private/scene/raster_scene_types.hpp`: Shared draw input types used by
  `RasterRenderer` and `PreviewRasterPipeline`.
- `raster/private/scene/raster_gpu_scene.hpp` / `raster/src/scene/raster_gpu_scene.cpp`: Owner of mesh,
  material, instance, light, texture, object-description, and light GPU
  resources; provides read-only spans for output passes and facade methods for
  scene upload/update; forwards RT acceleration-structure lifecycle and dirty
  updates into `RasterRtScene`.
- `raster/private/scene/raster_rt_scene.hpp` / `raster/src/scene/raster_rt_scene.cpp`: RT
  acceleration-structure mirror for the raster scene; builds BLAS from uploaded
  mesh buffers, builds/updates TLAS from renderer instances, applies per-instance
  trace masks for normal geometry versus height-scan ground geometry, and
  exposes the TLAS descriptor primitive for RT LiDAR and height-scan passes.
- `raster/private/scene/raster_scene_descriptors.hpp` /
  `raster/src/scene/raster_scene_descriptors.cpp`: Owner of scene descriptor bindings,
  pool, layout, set, and descriptor updates for frame uniforms, object
  descriptions, lights, tile uniforms, and textures.
- `raster/private/scene/raster_view_uniforms.hpp` / `raster/src/scene/raster_view_uniforms.cpp`: Owner of
  main frame uniforms, tile frame uniforms, camera array, main camera clip
  range, and uniform buffer update logic.
- `raster/private/core/raster_output_controller.hpp` /
  `raster/src/core/raster_output_controller.cpp`: Output orchestration layer that owns
  the preview AOV pipeline, tile atlas pass, LiDAR point cloud pass, height scan
  pass, and AOV texture query routing.
- `raster/private/output/preview_raster_pipeline.hpp` / `raster/src/output/preview/preview_raster_pipeline.cpp`:
  Main preview/offscreen AOV pipeline wrapper owning color/depth/id images,
  depth attachment, render pass, framebuffer, graphics pipelines, AOV texture
  export backing, dynamic frame-uniform descriptor binding, and draw command
  recording.
- `raster/include/raster/tile_config.hpp`: Pure raster tile output configuration contract,
  including enable flag, per-camera tile size, grid dimensions, and positive
  value normalization.
- `raster/private/output/tile/tile_aov_channel.hpp` / `raster/src/output/tile/tile_aov_channel.cpp`: Single-channel
  tile atlas output resource used by the multiview path; owns one atlas image,
  optional external-memory export, and layered image copies into row-major tile
  rects. `TileAtlasPass` holds separate color and depth channel atlases.
- `raster/private/output/tile/multiview_tile_targets.hpp` / `raster/src/output/tile/multiview_tile_targets.cpp`:
  Temporary layered color/depth-AOV/depth-attachment tile targets used by
  multiview tile batches before copying requested channels back into 2D atlases.
- `raster/private/output/tile/multiview_tile_raster_pipeline.hpp` /
  `raster/src/output/tile/multiview_tile_raster_pipeline.cpp`: Dedicated multiview tile
  render pass, raster pipeline, dome background pipeline, and draw recording.
- `raster/private/output/tile/tile_atlas_pass.hpp` / `raster/src/output/tile/tile_atlas_pass.cpp`: Owner of tile
  atlas orchestration state, multiview capability state, dirty/export-valid
  state, layered tile targets, tile atlas images, and multiview tile pipeline.
- `raster/include/raster/lidar_types.hpp`: Public Hydra-free LiDAR sensor,
  visualization, point, per-sensor cloud, and frame cloud contract.
- `raster/private/output/lidar/lidar_scan.hpp` /
  `raster/src/output/lidar/lidar_scan.cpp`: Pure CPU LiDAR sample count, angle,
  basis, beam direction, and per-sensor layout helpers.
- `raster/private/output/lidar/lidar_point_cloud_pass.hpp` /
  `raster/src/output/lidar/lidar_point_cloud_pass.cpp`: LiDAR output
  coordinator owning sensors, visualization config, global point buffer,
  sensor metadata buffer, CPU readback, Vulkan RT point generation, and overlay
  pass orchestration.
- `raster/private/output/lidar/lidar_depth_pipeline.hpp` /
  `raster/src/output/lidar/lidar_depth_pipeline.cpp`: Legacy single-sensor
  angular range raster pipeline. The active LiDAR point path now uses Vulkan RT
  ray queries through `lidar_points.comp`.
- `raster/private/output/lidar/lidar_point_generation_pipeline.hpp` /
  `raster/src/output/lidar/lidar_point_generation_pipeline.cpp`: Compute
  pipeline that binds the scene TLAS and traces one ray query per LiDAR beam
  into `LidarPointGpu` entries.
- `raster/private/output/lidar/lidar_point_overlay_pipeline.hpp` /
  `raster/src/output/lidar/lidar_point_overlay_pipeline.cpp`: Thin LiDAR
  compatibility wrapper around the shared point overlay pipeline.
- `raster/private/output/point_overlay/point_overlay_pipeline.hpp` /
  `raster/src/output/point_overlay/point_overlay_pipeline.cpp`: Shared preview
  point overlay graphics pipeline infrastructure for render pass, framebuffer,
  descriptor resources, push constants, and point-list draw binding. Sensor and
  point/sample layout details stay in overlay-specific vertex shaders.
- `raster/include/raster/height_scan_types.hpp`: Public Hydra-free height scan
  sensor, sample, per-sensor grid, and frame readback contract.
- `raster/private/output/height_scan/height_scan.hpp` /
  `raster/src/output/height_scan/height_scan.cpp`: Pure CPU height scan sample
  count, offset, basis, layout, and world-space origin helpers.
- `raster/private/output/height_scan/height_scan_generation_pipeline.hpp` /
  `raster/src/output/height_scan/height_scan_generation_pipeline.cpp`: Compute
  pipeline that binds the scene TLAS and traces one ground-mask ray query per
  height scan grid sample.
- `raster/private/output/height_scan/height_scan_pass.hpp` /
  `raster/src/output/height_scan/height_scan_pass.cpp`: Height scan output
  coordinator owning sensors, visualization config, GPU metadata, sample
  buffers, Vulkan RT generation, preview overlay orchestration, and CPU
  readback frames.
- `raster/src/output/tile/tile_atlas_renderer.cpp`: `RasterRenderer` compatibility forwarding
  for tile atlas recording and consume marking.
- `raster/src/scene/raster_runtime_updates.cpp`: `RasterRenderer` compatibility
  forwarding for runtime material and light updates into `RasterGpuScene`.
- `raster/src/scene/raster_geometry_upload.cpp`: `RasterRenderer` compatibility
  forwarding for instance and geometry updates into `RasterGpuScene`.
- `raster/src/scene/raster_scene_upload.cpp`: `RasterRenderer` compatibility forwarding
  for mesh-source and texture upload into `RasterGpuScene`, including
  `rebuildTextureResourcesAndSceneBindings` coordination across descriptors and
  output pipelines.
- `raster/src/core/raster_descriptor_sets.cpp`: `RasterRenderer` compatibility
  forwarding for descriptor creation/update and preview AOV recording through
  `RasterSceneDescriptors` and `RasterOutputController`.
- `raster/private/raster_image_barriers.hpp`: Vulkan image layout/barrier helpers.
- `raster/src/runtime/headless_vk.cpp`: Headless Vulkan offline app context support.

## Offscreen Rendering

- `raster/include/raster/aov_texture.hpp`: Public raster AOV contract, currently
  `color`, `depth`, `primId`, `instanceId`, `tileColor`, `tileDepth`,
  `tileDisplayColor`, and `tileDisplayDepth`.
- `raster/private/output/preview_raster_pipeline.hpp` / `raster/src/output/preview/preview_raster_pipeline.cpp`: Offscreen
  target allocation, resize refresh, AOV texture export backing, direct
  color/depth/id target selection, and framebuffer rebuilds.
- `raster/src/core/raster_output_controller.cpp`: `GetAovTexture` routing for standard
  preview AOVs and current tile atlas export images.
- `raster/private/output/tile/tile_aov_channel.hpp` / `raster/src/output/tile/tile_aov_channel.cpp`,
  `raster/private/output/tile/multiview_tile_targets.hpp` / `raster/src/output/tile/multiview_tile_targets.cpp`,
  `raster/private/output/tile/multiview_tile_raster_pipeline.hpp` /
  `raster/src/output/tile/multiview_tile_raster_pipeline.cpp`, and
  `raster/private/output/tile/tile_atlas_pass.hpp` /
  `raster/src/output/tile/tile_atlas_pass.cpp`: Per-channel tile atlas
  resources, layered multiview tile resources, layer-to-atlas copy,
  Hydra-exportable atlas color/depth sources, and local save suppression.
  `TileAtlasPass` intersects tile settings with the current Hydra tile AOV
  request mask before allocating and exporting channel atlases.

## Raster Baseline

- `raster/shaders/preview/raster.vert`: Active mesh vertex shader.
- `raster/shaders/preview/raster.frag`: Active mesh fragment shader for color,
  primitive ID, instance ID, depth AOV writes, and sphere/simple light shading
  from the renderer light buffer. DomeLight is parsed but not evaluated here.
- `raster/shaders/tile/tile_multiview.vert` /
  `raster/shaders/tile/tile_multiview.frag`: Mesh shader variants for multiview
  tile rendering using `gl_ViewIndex` and `TileFrameUniforms`.
- `raster/shaders/tile/dome_background_tile_multiview.vert` /
  `raster/shaders/tile/dome_background_tile_multiview.frag`: Dome background
  variants for multiview tile rendering.
- `raster/shaders/lidar/lidar_depth.vert` /
  `raster/shaders/lidar/lidar_depth.frag`: Legacy mesh variants that project
  scene geometry into a LiDAR angular range image.
- `raster/shaders/lidar/lidar_points.comp`: Active Vulkan RT LiDAR compute
  shader; performs one ray query against the scene TLAS per beam and writes the
  global LiDAR point buffer.
- `raster/shaders/height_scan/height_scan.comp`: Active Vulkan RT height scan
  compute shader; traces only TLAS instances carrying the ground trace mask and
  writes height scan sample buffers for CPU readback.
- `raster/shaders/lidar/lidar_overlay.vert` /
  `raster/shaders/lidar/lidar_overlay.frag`: Preview point overlay shaders for
  valid LiDAR hits.
- `raster/shaders/height_scan/height_scan_overlay.vert` /
  `raster/shaders/height_scan/height_scan_overlay.frag`: Preview point overlay
  shaders for valid height scan hit samples, drawn from generated GPU sample
  buffers after preview rendering.
- `raster/shaders/common/host_device.h`: Shared raster descriptor bindings,
  material structs, frame uniforms, tile multiview uniforms, LiDAR and height
  scan GPU structs, trace mask constants, and push constants.

## Hydra Delegate

- `hdRobot/renderDelegate.h` / `hdRobot/renderDelegate.cpp`: Hydra delegate
  ownership, supported primitives, render param setup, tile render setting
  descriptors including per-channel tile color/depth enable flags, LiDAR and
  height scan visualization render settings, standard and tile AOV descriptors,
  and resource access.
- `hdRobot/renderPass.h` / `hdRobot/renderPass.cpp`: Hydra render pass object
  and frame execution entry into the bridge.
- `hdRobot/rasterBridge.h` / `hdRobot/rasterBridge.cpp`:
  Converts Hydra frame state into `RasterRenderer` updates and render calls,
  including the all-camera snapshot passed to raster renderer, the current
  frame's requested tile AOV channel mask, sorted LiDAR and height scan sensor
  forwarding, LiDAR and height scan visualization config forwarding,
  `hdRobot:traceRole = "ground"` routing into raster instance trace masks, and
  ordered AOV copy groups that copy fixed tile AOVs before display tile AOVs
  and other AOVs.
- `hdRobot/renderParam.h` / `hdRobot/renderParam.cpp`: Shared Hydra render
  parameter object, camera array, LiDAR and height scan sensor arrays, tile
  config, LiDAR and height scan visualization config, scene dirty flags,
  texture registry, and renderer bridge ownership.
- `hdRobot/tokens.h` / `hdRobot/tokens.cpp`: Hydra token definitions,
  including tile render settings, LiDAR/height scan visualization settings,
  LiDAR/height scan params, custom `lidarSensor` and `heightScanSensor` sprim
  types, mesh `hdRobot:traceRole`, `ground`, and tile AOV tokens.
- `hdRobot/plugInfo.json`: Hydra render delegate plugin metadata. The custom
  sensor USD schema and adapter metadata now live in `hdRobotLidarUsd/`.

## USD Sensor Recognition

- `hdRobotLidarUsd/plugInfo.json`: Standalone plugin metadata for
  `HdRobotLidarSensorSchema`, `HdRobotLidarSensorAdapter`,
  `HdRobotHeightScanSensorSchema`, and `HdRobotHeightScanSensorAdapter`.
- `hdRobotLidarUsd/generatedSchema.usda`: Codeless concrete `LidarSensor` and
  `HeightScanSensor` schema definitions and fallback typed sensor attributes.
- `hdRobotLidarUsd/lidarSensorAdapter.h` /
  `hdRobotLidarUsd/lidarSensorAdapter.cpp`: UsdImaging adapter for USD
  `LidarSensor`; inserts the custom `lidarSensor` sprim, forwards sensor
  attributes, and handles transform/dependency dirtying. It uses the literal
  Hydra sprim type token so this USD recognition plugin does not link against
  the `hdRobot` render delegate target.
- `hdRobotLidarUsd/heightScanSensorAdapter.h` /
  `hdRobotLidarUsd/heightScanSensorAdapter.cpp`: UsdImaging adapter for USD
  `HeightScanSensor`; inserts the custom `heightScanSensor` sprim, forwards
  sensor attributes, and handles transform/dependency dirtying without
  linking against the `hdRobot` render delegate target.

## Hydra Scene Primitives

- `hdRobot/mesh.h` / `hdRobot/mesh.cpp`: Mesh sync, topology/primvar analysis,
  tangent generation, material binding, visibility, transforms,
  `hdRobot:traceRole = "ground"` ingestion, and conversion into renderer mesh
  data.
- `hdRobot/material.h` / `hdRobot/material.cpp`: Material sync lifecycle,
  MaterialX parser invocation, texture registration, and dirty marking.
- `hdRobot/materialXParser.h` / `hdRobot/materialXParser.cpp`: USD Preview
  Surface plus MaterialX surface selection, `ND_surface` closure traversal,
  standard surface/OpenPBR and selected BSDF/EDF input rules, upstream texture
  and primvar-reader traversal, texture binding metadata, and `HydraMaterial`
  field mapping.
- `hdRobot/camera.h` / `hdRobot/camera.cpp`: Camera sync and camera data
  conversion. Sensor parameters are no longer read from camera prims.
- `hdRobot/lidarSensor.h` / `hdRobot/lidarSensor.cpp`: Hydra sprim for custom
  USD `LidarSensor` prims; reads unprefixed typed sensor attributes from
  `/lidars/*`, resolves sensor transform, and upserts `HdRobotLidarSensorData`.
- `hdRobot/heightScanSensor.h` / `hdRobot/heightScanSensor.cpp`: Hydra sprim
  for custom USD `HeightScanSensor` prims; reads unprefixed typed sensor
  attributes, resolves sensor transform, and upserts `HdRobotHeightScanSensorData`.
- `hdRobot/light.h` / `hdRobot/light.cpp`: Light sync and renderer light data.
- `hdRobot/points.h` / `hdRobot/points.cpp`: HdPoints sync surface; the
  minimal raster path currently does not draw points.
- `hdRobot/instancer.h` / `hdRobot/instancer.cpp`: Hydra instancer support.
- `hdRobot/sceneData.h` / `hdRobot/sceneData.cpp`: Shared scene data helpers.

## Render Buffers And Texture Export

- `hdRobot/renderBuffer.h` / `hdRobot/renderBuffer.cpp`: Hydra render buffer
  allocation, Hgi texture descriptor creation, GL interop, and texture format
  conversion.
- `hdRobot/hydraTextureAssetExport.h` / `hdRobot/hydraTextureAssetExport.cpp`:
  Resolves registered Hydra material texture assets and exports encoded bytes
  for raster upload.
- `hdRobot/hydraRasterAovCopy.h` / `hdRobot/hydraRasterAovCopy.cpp`: Maps Hydra
  AOV tokens to `RasterAov` values and copies exported raster AOV textures into
  Hydra render buffers through OpenGL interop; fixed `tileColor` and
  `tileDepth` copies require exact atlas-size render buffers, while display tile
  AOVs can use GL blit scaling.
- `hdRobot/glInteropCache.h` / `hdRobot/glInteropCache.cpp`: Hydra-owned
  Vulkan external-memory import cache for raster AOV textures exposed as
  source OpenGL textures.
- `hdRobot/utils.h` / `hdRobot/utils.cpp`: Utility functions shared by the
  Hydra plugin.

## Model And Scene Loading

- `common/ModelLoader.h`: Abstract model loading interface consumed by
  `RasterRenderer::uploadMeshFromLoader`, including legacy texture filenames and
  in-memory encoded texture assets.
- `common/obj_loader.h` / `common/obj_loader.cpp`: OBJ loader implementation,
  vertices, indices, normals, texcoords, material assignment, and fallback
  normals.

## Shader Files

- `raster/shaders/common/host_device.h`: Shared host/device structs and constants.
  `FrameUniforms` carries one camera/light-count slot selected by dynamic
  uniform offset; `TileFrameUniforms` carries per-batch multiview tile cameras;
  `PushConstantRaster` carries per-draw model/object/instance data for the
  raster path.
- `raster/shaders/preview/raster.vert`: Minimal raster vertex shader for mesh
  positions, normals, vertex colors, texcoords, tangents, and per-instance
  transform/object metadata.
- `raster/shaders/preview/raster.frag`: Minimal raster fragment shader writing color,
  primId, instanceId, normalized depth AOV attachments, and sphere/simple light
  diffuse shading from the shared light buffer.
- `raster/shaders/tile/tile_multiview.vert` /
  `raster/shaders/tile/tile_multiview.frag`: Multiview tile mesh shader variants
  that read camera arrays from `TileFrameUniforms`.
- `raster/shaders/tile/dome_background_tile_multiview.vert` /
  `raster/shaders/tile/dome_background_tile_multiview.frag`: Multiview tile dome
  background variants that reconstruct view directions per `gl_ViewIndex`.

## Question Routing

- Frame rendering:
  `raster/src/core/raster_frame_executor.cpp`, `raster/src/core/raster_renderer.cpp`,
  `raster/src/core/raster_output_controller.cpp`, `raster/src/output/tile/tile_atlas_pass.cpp`,
  `raster/src/output/lidar/lidar_point_cloud_pass.cpp`, `hdRobot/rasterBridge.cpp`,
  then search for `RenderRasterFrame`, `recordFramePasses`,
  `recordPreviewAovs`, `recordTileAtlas`, `recordLidarPointClouds`,
  `recordLidarPointOverlay`, `recordFrameUniformUpdate`, and `updateScene`.
- Resize or render target size:
  `raster/src/core/raster_renderer.cpp`, `raster/include/raster/raster_renderer.hpp`,
  `raster/src/output/preview/preview_raster_pipeline.cpp`, `raster/src/runtime/raster_session.cpp`, then search
  for `onResize`, `createPreviewAovTargets`, `recreateAovTargets`, `getRenderSize`, and
  `ensureRasterSessionReady`.
- Offscreen AOV export:
  `raster/include/raster/aov_texture.hpp`, `raster/src/output/preview/preview_raster_pipeline.cpp`,
  `raster/src/core/raster_output_controller.cpp`, `raster/src/output/tile/tile_atlas_pass.cpp`,
  `raster/src/output/tile/tile_aov_channel.cpp`,
  `hdRobot/hydraRasterAovCopy.cpp`, then search for `GetAovTexture`,
  `getAovTexture`, `setRequestedTileAovChannels`, `RasterAov`, and
  `CopyAovToRenderBuffer`.
- Raster pipeline:
  `raster/private/output/preview_raster_pipeline.hpp`, `raster/src/output/preview/preview_raster_pipeline.cpp`,
  `raster/src/core/raster_descriptor_sets.cpp`, `raster/shaders/preview/raster.vert`,
  `raster/shaders/preview/raster.frag`, then search for `PreviewRasterPipeline`,
  `createGraphicsPipeline`, `createFramebuffer`, `recordPreviewAovs`, and
  `PushConstantRaster`.
- Hydra render flow:
  `hdRobot/renderPass.cpp`, `hdRobot/rasterBridge.cpp`,
  `hdRobot/renderDelegate.cpp`, then search for `RenderRasterFrame`,
  `_UpdateRenderScene`, and `Sync`.
- Hydra camera and multi-camera flow:
  `hdRobot/camera.cpp`, `hdRobot/renderParam.h`,
  `hdRobot/rasterBridge.cpp`, `raster/include/raster/raster_renderer.hpp`,
  `raster/src/scene/raster_view_uniforms.cpp`, and `raster/src/output/tile/tile_atlas_pass.cpp`, then
  search for `v_camera`,
  `v_lidarSensor`, `v_heightScanSensor`, `GetCamerasSnapshot`, `GetLidarSensorsSnapshot`,
  `GetHeightScanSensorsSnapshot`,
  `setCameras`, `setMainCamera`, `setTileConfig`, `setRequestedTileAovChannels`,
  `recordTileAovAtlas`, and `RasterCameraSpec`.
- LiDAR point cloud generation or overlay:
  `raster/include/raster/lidar_types.hpp`,
  `raster/src/output/lidar/lidar_scan.cpp`,
  `raster/src/output/lidar/lidar_point_cloud_pass.cpp`,
  `raster/src/output/lidar/lidar_point_generation_pipeline.cpp`,
  `raster/src/output/lidar/lidar_point_overlay_pipeline.cpp`,
  `raster/src/scene/raster_rt_scene.cpp`,
  `raster/shaders/lidar/lidar_points.comp`,
  `raster/shaders/lidar/lidar_overlay.vert`,
  `hdRobot/renderDelegate.cpp`, and `hdRobot/rasterBridge.cpp`, then search
  for `RasterLidarSensorSpec`, `LidarPointCloudPass`,
  `GenerateLidarPointClouds`, `OverlayLidarPointCloud`,
  `hdRobot:lidar:visualize`, `readLidarPointCloudFrame`, and
  `BuildLidarScanLayout`.
  For Hydra-side LiDAR discovery, start with
  `hdRobotLidarUsd/lidarSensorAdapter.cpp`,
  `hdRobotLidarUsd/plugInfo.json`,
  `hdRobotLidarUsd/generatedSchema.usda`, `hdRobot/lidarSensor.cpp`,
  `hdRobot/renderDelegate.cpp`, and `hdRobot/rasterBridge.cpp`, then search
  for `LidarSensor`, `schemaIdentifier`, `lidarSensor`,
  `UpsertLidarSensor`, and `GetLidarSensorsSnapshot`.
- Height scan generation or overlay:
  `raster/include/raster/height_scan_types.hpp`,
  `raster/private/output/height_scan/height_scan.hpp`,
  `raster/src/output/height_scan/height_scan.cpp`,
  `raster/private/output/height_scan/height_scan_generation_pipeline.hpp`,
  `raster/src/output/height_scan/height_scan_generation_pipeline.cpp`,
  `raster/private/output/height_scan/height_scan_pass.hpp`,
  `raster/src/output/height_scan/height_scan_pass.cpp`,
  `raster/shaders/height_scan/height_scan.comp`,
  `raster/shaders/height_scan/height_scan_overlay.vert`,
  `raster/shaders/height_scan/height_scan_overlay.frag`,
  `raster/src/output/point_overlay/point_overlay_pipeline.cpp`,
  `raster/src/core/raster_frame_executor.cpp`,
  `raster/src/core/raster_output_controller.cpp`,
  `hdRobotLidarUsd/heightScanSensorAdapter.cpp`,
  `hdRobotLidarUsd/plugInfo.json`,
  `hdRobotLidarUsd/generatedSchema.usda`,
  `hdRobot/heightScanSensor.cpp`, `hdRobot/renderDelegate.cpp`,
  `hdRobot/rasterBridge.cpp`, and
  `raster/src/scene/raster_rt_scene.cpp`, then search for
  `RasterHeightScanSensorSpec`, `RasterHeightScanVisualizationConfig`,
  `HeightScanPass`, `GenerateHeightScans`, `OverlayHeightScans`,
  `hdRobot:heightScan:visualize`, `hdRobot:traceRole`,
  `kRasterTraceMaskGround`, `RASTER_TRACE_MASK_GROUND`,
  `HeightScanSensor`, `heightScanSensor`, `UpsertHeightScanSensor`,
  `setHeightScanSensors`, `setHeightScanVisualizationConfig`, and
  `readHeightScanFrame`.
- Hydra mesh sync:
  `hdRobot/mesh.cpp`, `hdRobot/mesh.h`, `hdRobot/tokens.h`,
  `hdRobot/rasterBridge.cpp`, and `raster/private/scene/raster_scene_types.hpp`,
  then search for `_CreateGiMeshes`, `_UpdateGeometry`, `_AnalyzePrimvars`,
  `traceRole`, `ground`, `traceMask`, and tangent calculation helpers.
- Material or texture path issues:
  `hdRobot/material.cpp`, `raster/src/scene/raster_gpu_scene.cpp`,
  `hdRobot/hydraTextureAssetExport.cpp`, `common/ModelLoader.h`,
  `common/obj_loader.cpp`, then search for `GetTexturePath`,
  `ExportRegisteredTextures`, `stbi_load_from_memory`, `loadMaterial`, and
  material buffer updates.
- PBR material struct or shader material layout:
  `common/data_loader.h`, `hdRobot/sceneData.h`, `hdRobot/sceneData.cpp`,
  `hdRobot/rasterBridge.cpp`, `raster/shaders/common/host_device.h`,
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
  `common/ModelLoader.h`, `hdRobot/sceneData.h`,
  `hdRobot/renderParam.cpp`, `hdRobot/hydraTextureAssetExport.cpp`,
  `raster/src/scene/raster_gpu_scene.cpp`, then search for `TextureUsage`,
  `TextureColorSpaceForUsage`, `GetTextureAssets`, and
  `VK_FORMAT_R8G8B8A8_UNORM`.
- Raster shader material sampling:
  `raster/shaders/preview/raster.frag`, `raster/shaders/common/host_device.h`, then
  search for `sampleBaseColor`, `WaveFrontMaterial`, and
  `baseColorTextureId`.
- Raster light evaluation:
  `hdRobot/light.cpp`, `hdRobot/sceneData.cpp`,
  `hdRobot/rasterBridge.cpp`, `raster/src/scene/raster_runtime_updates.cpp`,
  `raster/src/scene/raster_gpu_scene.cpp`, `raster/src/scene/raster_scene_descriptors.cpp`,
  `raster/shaders/common/host_device.h`, `raster/shaders/preview/raster.frag`, then search
  for `HydraLight::toLight`, `updateLights`, `updateLightBuffer`, `eLights`,
  and `evaluateSphereLight`.
- Normal map data flow:
  `hdRobot/mesh.cpp`, `hdRobot/sceneData.h`, `hdRobot/sceneData.cpp`,
  `common/data_loader.h`, `raster/shaders/common/host_device.h`,
  `raster/shaders/preview/raster.vert`, then search for `tangent`,
  `bitangentSigns`, and `normalTextureId`.
- Advanced MaterialX inputs:
  `hdRobot/materialXParser.cpp`, `hdRobot/sceneData.h`,
  `hdRobot/sceneData.cpp`, `common/data_loader.h`,
  `raster/shaders/common/host_device.h`, then search for `transmissionFactor`,
  `transmissionColorFactor`, `subsurfaceFactor`, `subsurfaceTextureId`, and
  the matching material parser fields.
- Instancers or material subsets:
  `hdRobot/instancer.cpp`, `hdRobot/mesh.cpp`,
  `hdRobot/sceneData.cpp`, `hdRobot/rasterBridge.cpp`, then search
  for `ComputeFlattenedTransforms`, `GetGeomSubsets`, `scene_mat_ids`,
  `materialIds`, and `rendererInstanceIds`.
- Render buffer or Hgi texture issues:
  `hdRobot/renderBuffer.cpp`, `hdRobot/renderBuffer.h`, then search for
  `createDesc`, `ConvertToHgiTexture`, `Allocate`, and `GetFormat`.
- Model loading:
  `common/obj_loader.cpp`, `common/obj_loader.h`, `common/ModelLoader.h`,
  `raster/src/scene/raster_gpu_scene.cpp`, `raster/src/scene/raster_scene_upload.cpp`, then
  search for `loadModel`,
  `loadVertices`, `loadIndices`, and `assignMaterialIndices`.
- Build or install failures:
  Root `CMakeLists.txt`, relevant subdirectory `CMakeLists.txt`,
  `install.sh`, and the failing command output.

## Graphify Anchors

The graph report currently identifies these high-connectivity anchors:

- `GetOrImportSourceGlTexture()`
- `_CreateGiMeshes()`
- `ensureResources()`
- `createDesc()`
- `RenderRasterFrame()`
- `destroy()`
- `ensureRasterSessionReady()`
- `updateScene()`
- `Sync()`
- `_ProcessPrimvar()`

When a question is ambiguous, start from the matching anchor, inspect its
definition, then read direct callers/callees before widening to the full
community.
