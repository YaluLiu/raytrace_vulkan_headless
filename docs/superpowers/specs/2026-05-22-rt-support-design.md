# RT Support Design

## Goal

Add ray tracing infrastructure to the raster renderer so later LiDAR work can trace against the existing Hydra mesh scene without replacing the current raster preview, tile, or LiDAR paths in this session.

## Scope

This change adds Vulkan ray tracing capability, bottom-level acceleration structure (BLAS) construction for uploaded meshes, top-level acceleration structure (TLAS) construction for renderer instances, runtime BLAS/TLAS synchronization, and a minimal TLAS descriptor interface.

This change does not add a LiDAR ray tracing pass, ray tracing shaders, shader binding tables, or replacement behavior for the current range-raster LiDAR pipeline.

## Existing Context

The current renderer uploads Hydra meshes through `HdRobotRasterBridge::uploadInitialScene()`, converts each `HydraMesh` to a `ModelLoader`, uploads mesh buffers through `RasterRenderer::uploadMeshFromLoader()`, and stores GPU buffers and instances in `RasterGpuScene`.

Runtime Hydra changes are split into geometry and instance dirty paths:

- Geometry changes call `RasterRenderer::updateMeshGeometry(meshId)`.
- Instance, transform, visibility, and render-tag changes call `RasterRenderer::updateInstance(rendererInstanceId, transform, visible)`.

The headless ray tracing project uses the same conceptual mesh shape: each mesh owns vertex and index buffers, each renderer instance points at an object index, BLAS inputs are created from buffer device addresses, and TLAS instances mirror Hydra instance slots. This design keeps that structure to reduce conversion risk.

## Architecture

Add a small RT scene component owned by `RasterGpuScene`. `RasterGpuScene` remains the source of truth for uploaded mesh buffers and instances; the RT component mirrors those buffers into acceleration structures.

The RT component is responsible for:

- Initializing `nvvk::RaytracingBuilderKHR` with the renderer device, allocator, and graphics queue family.
- Creating `VkAccelerationStructureGeometryKHR` triangle inputs from `RasterMeshBuffers`.
- Building one BLAS per mesh source.
- Building one TLAS instance per `RasterInstance`.
- Updating instance masks from renderer visibility.
- Rebuilding or updating acceleration structures after geometry and instance changes.
- Exposing `VkAccelerationStructureKHR` for descriptor writes.

`RasterRenderer` exposes a narrow facade:

- `createRayTracingResources()`
- `destroyRayTracingResources()`
- `flushRayTracingUpdates()`
- `getRayTracingTlas()`
- `getRayTracingTlasDescriptorInfo()`

The exact descriptor helper can return either the TLAS handle plus `VkWriteDescriptorSetAccelerationStructureKHR` data, or a small value object that lets the future LiDAR RT pass write an acceleration-structure descriptor without knowing the builder internals.

## Vulkan Requirements

`RasterSession::setupContext()` must request the RT device features and extensions needed by `nvvk::RaytracingBuilderKHR`:

- `VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME`
- `VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME`
- `VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME`
- `VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME` if required by the local Vulkan headers and context helper

Even though this session does not create RT pipelines, requesting `VK_KHR_RAY_TRACING_PIPELINE` keeps compatibility with the headless reference and the next LiDAR RT pass. If a device cannot provide these features, session initialization should fail with a message that names ray tracing support instead of failing later during AS construction.

Mesh vertex and index buffers must include:

- `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`
- `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`
- `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR`

The existing shader-device-address usage is already present for raster shader access; the AS build input flag is the missing usage for BLAS construction.

## Resource Lifecycle

Initial creation:

1. `HdRobotRasterBridge::initializeRasterSession()` uploads textures and meshes as today.
2. `RasterSession::createRenderResources()` creates normal raster resources as today.
3. `RasterSession::createRenderResources()` also creates RT resources after mesh upload and before per-frame rendering.
4. RT creation builds BLAS from all uploaded mesh buffers and TLAS from all renderer instances.

Per-frame update:

1. `HdRobotRasterBridge::updateGeometry()` updates changed mesh loaders and GPU mesh buffers.
2. `RasterGpuScene::updateMeshGeometry(meshId)` marks the matching BLAS dirty.
3. `HdRobotRasterBridge::updateInstances()` updates renderer instance transforms and visibility.
4. `RasterGpuScene::updateInstance()` marks TLAS dirty.
5. `RasterSession::render()` calls `RasterRenderer::flushRayTracingUpdates()` before recording frame passes.
6. `flushRayTracingUpdates()` rebuilds dirty BLAS data when needed, then updates or rebuilds TLAS once.

Destruction:

1. `RasterRenderer::destroyResources()` destroys output, descriptors, uniforms, scene, and RT resources in an order that leaves no acceleration-structure references alive after mesh buffers are destroyed.
2. RT resources are destroyed before mesh buffers and allocator teardown.

## BLAS Strategy

Each mesh source maps to one BLAS. Triangle geometry uses:

- Vertex format: `VK_FORMAT_R32G32B32_SFLOAT`
- Vertex stride: `sizeof(VertexObj)`
- Index type: `VK_INDEX_TYPE_UINT32`
- Primitive count: `nbIndices / 3`
- Max vertex: `nbVertices - 1` when `nbVertices > 0`

Empty or invalid meshes are skipped or represented by no BLAS input. The TLAS builder must not create a visible instance for a mesh without a valid BLAS.

For geometry updates, correctness is more important than in-place update performance. The implementation should use a conservative rebuild path when vertex or index counts change. If the local `nvvk::RaytracingBuilderKHR::updateBlas()` can safely update the existing geometry, it can be used only when the input shape remains compatible. Otherwise the RT component should wait for the device to be idle and rebuild the BLAS/TLAS set from current mesh buffers.

## TLAS Strategy

Each `RasterInstance` maps to one `VkAccelerationStructureInstanceKHR`.

TLAS fields:

- `transform`: `nvvk::toTransformMatrixKHR(instance.transform)`
- `instanceCustomIndex`: renderer instance index, so future ray payloads can recover the hit renderer instance
- `accelerationStructureReference`: BLAS device address for `instance.objIndex`
- `flags`: `VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR`
- `mask`: `0xFF` when visible, `0x00` when hidden or invalid
- `instanceShaderBindingTableRecordOffset`: `0` for all triangle meshes in this session

The headless reference uses `objIndex` as the custom index. This renderer already preserves separate renderer instance IDs for Hydra instance slots, and the future multi-LiDAR path will likely need per-instance filtering or reporting. Therefore this design uses renderer instance index as the custom index while keeping `objIndex` for BLAS lookup.

## Descriptor Interface

The RT support should provide only the TLAS descriptor primitive needed by future passes:

- A method to query whether TLAS is valid.
- A method to get the `VkAccelerationStructureKHR`.
- A helper that fills `VkWriteDescriptorSetAccelerationStructureKHR` for a caller-provided descriptor write.

This session should not allocate a LiDAR RT descriptor pool, create a LiDAR RT descriptor set, or bind RT descriptors in command buffers. Those belong to the next LiDAR pass.

## Error Handling

If RT resources are requested before mesh upload, the build should create an empty invalid state and log a clear warning, not crash.

If the Vulkan device lacks RT features, session setup should fail early with a ray-tracing-specific error.

If a mesh has zero vertices, fewer than three indices, or a null buffer, the BLAS input for that mesh should be skipped and its TLAS instances should be masked out.

## Testing And Verification

Primary verification:

- Configure/build the project after adding headers and sources.
- Confirm Vulkan context creation requests the RT extensions.
- Confirm existing raster frame paths still compile and do not require LiDAR RT resources.
- Confirm graphify code graph is rebuilt after code modifications.

Focused code checks:

- `RasterGpuScene::uploadMeshFromLoader()` creates vertex/index buffers with AS build input usage.
- `RasterGpuScene::updateMeshGeometry()` preserves AS build input usage when reallocating buffers.
- `RasterGpuScene::updateInstance()` marks TLAS dirty.
- `RasterSession::render()` flushes RT updates before existing frame passes.

## Documentation

Update `docs/FILEMAP.md` after implementation to include the RT scene component and the new renderer facade methods.
