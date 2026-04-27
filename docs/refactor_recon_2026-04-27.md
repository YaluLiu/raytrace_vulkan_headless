# Refactor Recon - 2026-04-27

## How to Continue in a New Conversation

Start by reading this document and `graphify-out/GRAPH_REPORT.md`.

The goal of the next work session is refactoring for module decoupling, code
style consistency, readability, and safer future changes. Do not start with
large behavioral rewrites. First strengthen verification, then do small
behavior-preserving refactors.

Current branch when this report was written:

- `split-dlss-lowres-highres-aov`
- Main graphify hotspots: `resetAccumulation()`, `createBVH()`, `resize()`,
  `_Execute()`, `app_init_or_resize()`, `_ProcessPrimvar()`,
  `_CreateGiMeshes()`
- Important user preference: prefer subagents for independent codebase
  reconnaissance or implementation slices.

Project rule: after modifying code files, run:

```bash
python3 -c "from graphify.watch import _rebuild_code; from pathlib import Path; _rebuild_code(Path('.'))"
```

This document is a report only, so graph rebuild was not required for creating
it.

## Executive Summary

The main refactor problem is not file count. The main problem is that shared
mutable state and GPU resource lifecycle boundaries are unclear.

The first refactor wave should avoid behavior changes:

1. Fix verification scripts and add CTest entry points.
2. Clean obvious readability issues: broken comments, inconsistent formatting,
   naming, stale debug output.
3. Extract low-risk helper modules: scene data, settings value reading, texture
   resolving, render target descriptors.
4. Encapsulate public mutable state gradually.
5. Only then split the renderer into pass/backend abstractions.

## Current Module Map

### `headless`

`headless` owns the Vulkan ray tracing renderer, render targets, acceleration
structures, DLSS-RR integration, LiDAR passes, GL interop textures, Hydra update
entry points, and standalone demo runtime.

Main files:

- `headless/hello_vulkan.hpp`: central state holder. This is currently the
  largest architectural bottleneck.
- `headless/ray_trace_app.cpp`: application/session wrapper around
  `HelloVulkan`.
- `headless/hello_vulkan.cpp`: setup, frame uniforms, render target creation,
  DLSS execution, resource destruction, resize.
- `headless/hello_vulkan_mesh.cpp`: model upload, BLAS/TLAS, spheres,
  runtime geometry update.
- `headless/hello_vulkan_pipeline.cpp`: main ray tracing descriptor, pipeline,
  SBT, raytrace dispatch.
- `headless/hello_vulkan_lidar.cpp`: LiDAR RT pass and composite compute pass.
- `headless/hello_vulkan_hydra.cpp`: runtime material/light/instance updates
  used by Hydra.

The class split in `.cpp` files is useful, but the state is still centralized in
`HelloVulkan`.

### `hdRobot`

`hdRobot` is the Hydra render delegate/plugin layer. It translates Hydra scene
state into the headless renderer.

Main files:

- `hdRobot/renderDelegate.cpp`: Hydra object factories and render settings.
- `hdRobot/renderParam.h/.cpp`: shared Hydra scene state.
- `hdRobot/renderPass.cpp`: strongest bridge to `RayTraceApp`.
- `hdRobot/mesh.cpp`: mesh sync, primvar processing, triangulation, material
  subset mapping.
- `hdRobot/material.cpp`, `light.cpp`, `camera.cpp`, `points.cpp`: Hydra prim
  sync into shared state.
- `hdRobot/renderBuffer.cpp`: CPU buffer, Hgi texture, GL readback.

The biggest issue is `HdRobotRenderParam`: it is both a Hydra render param and a
public mutable scene database.

### `common`, `demo`, scripts, CMake

`common` holds OBJ/model data types. `demo` includes standalone demo logic and
USD loading. Scripts provide contract checks and visual regression, but several
checks are currently stale or not portable on Windows.

Build isolation is incomplete: root CMake always adds `headless`, `demo`, and
`hdRobot`, and several dependencies leak through `PUBLIC` target settings.

## Coupling Hotspots

### `HelloVulkan` God Class

`headless/hello_vulkan.hpp` mixes:

- host model data (`m_Loader`, `m_objModel`, `m_objDesc`, `m_instances`)
- descriptor state
- RT pipeline and SBT state
- DLSS state
- LiDAR pipeline/composite state
- offscreen images and GL interop textures
- Hydra lights, instance ids, runtime material updates
- implicit sphere geometry

Refactor direction:

- Extract `RenderTargets` first.
- Then extract `RayTracingScene`.
- Then extract `DlssPass` and `LidarPass`.
- Keep `HelloVulkan` as an orchestration facade until consumers are migrated.

### `RayTraceApp::createBVH()`

`headless/ray_trace_app.cpp:268` is named `createBVH()`, but it creates nearly
all render resources:

- offscreen render targets
- DLSS
- light and instance buffers
- scene descriptors
- uniforms
- BLAS/TLAS
- RT pipeline/SBT
- LiDAR RT and composite pipelines

Refactor direction:

- Keep the public function initially to avoid broad call churn.
- Split internals into named private helpers:
  `createRenderTargets()`, `createSceneDescriptors()`,
  `createAccelerationStructures()`, `createRayTracingPasses()`,
  `createPostPasses()`.
- Rename the public method only after call sites are contained.

### `HelloVulkan::createOffscreenRender()`

`headless/hello_vulkan.cpp:508` is a resize/DLSS/AOV/LiDAR/GL interop knot.

Refactor direction:

- Add a render target descriptor table with:
  name, format, extent kind, Vulkan usage, optional GL interop format/filter.
- Centralize descriptor refresh after render target resize.
- Extract duplicate logic currently present in
  `refreshOffscreenRenderTargetsIfNeeded()` and `onResize()`.

### `HdRobotRenderPass::_Execute()`

`hdRobot/renderPass.cpp:281` does too much in one frame path:

- render tag changes
- AOV and size detection
- render settings application
- app init/resize
- main camera and LiDAR camera sync
- light sync
- BLAS/TLAS/material updates
- render
- AOV copy

Refactor direction:

- Extract `HeadlessRenderBridge` around `RayTraceApp`.
- Let `HdRobotRenderPass` collect Hydra frame inputs and call bridge methods.
- Keep behavior identical at first.

### `HdRobotRenderParam`

`hdRobot/renderParam.h` stores public mutable scene vectors:

- `v_mesh`
- `v_mat`
- `v_light`
- `v_sphere`
- `textureRegistry`

Refactor direction:

- Move `HydraMesh`, `HydraMaterial`, `HydraLight`, `TextureRegistry`, and
  `ConvertVmeshToLoader()` into a `sceneData` or `hydraScene` file.
- Add accessor/mutation methods before making fields private.
- Later replace vector mutation plus dirty flags with explicit scene deltas.

### `HdRobotMesh`

`hdRobot/mesh.cpp` combines:

- dirty bit handling
- triangulation
- primvar discovery and conversion
- normal/tangent generation
- material binding and geom subset mapping
- writing renderer-facing `HydraMesh`

Refactor direction:

- Extract primvar processing into testable functions.
- Extract matrix conversion helpers.
- Remove stale commented-out implementation blocks.
- Avoid calling Hydra scene delegate methods from worker threads unless thread
  safety is confirmed.

## Code Style and Readability Issues

### Broken Encoding in Comments

Many Chinese comments are mojibake. Examples:

- `headless/ray_trace_app.cpp`
- `headless/hello_vulkan.cpp`
- `hdRobot/mesh.cpp`
- `hdRobot/renderBuffer.cpp`
- `install.sh`
- `README_CN.md`

Recommendation:

- Do not mass rewrite all comments at once.
- When touching a file, fix nearby corrupted comments or replace them with short
  English comments.
- For pure comment cleanup, commit separately from behavior changes.

### Naming Inconsistency

Examples:

- `m_Loader` vs normal member style
- `mesh_Id`
- `UpdateCamera()`
- `_cleaned`
- `app_anim_real()`
- local variables named `_mesh`

Recommendation:

- Avoid global renaming first.
- Rename only inside modules being actively refactored.
- Prefer behavior-preserving renames with compiler coverage.

### Logging Inconsistency

Mixed styles:

- `std::cout`
- `std::cerr`
- `printf`
- `TF_WARN`
- `TF_RUNTIME_ERROR`

Recommendation:

- In `hdRobot`, prefer USD diagnostic macros.
- In standalone `headless`, introduce small logging helpers or use the existing
  DLSS logging pattern where appropriate.

### Public Mutable State

High-risk public state:

- `HelloVulkan` public vectors and Vulkan resources
- `HdRobotRenderParam` public scene vectors
- `ModelLoader` public data and misleading loader interface

Recommendation:

- Add narrow methods first.
- Migrate call sites.
- Then make state private.

## Verification Status and Gaps

Commands checked during reconnaissance:

```bash
python scripts/check_dlss_rr_shader_contracts.py
python scripts/check_dlss_rr_supported_size.py
python scripts/check_lidar_after_dlss_contracts.py
```

Observed status:

- `check_dlss_rr_shader_contracts.py` failed because it still expects
  `eRaygenPassLowResBeautyWithLidar` in the pipeline, while current code uses
  `eRaygenPassLowResBeauty` and separate LiDAR composite.
- `check_dlss_rr_supported_size.py` failed on Windows due to default GBK file
  decoding. It should read with `encoding="utf-8"`.
- `check_lidar_after_dlss_contracts.py` is currently untracked and stale; it
  expects `eFinalImage` and old `compositeLidarPointCloud` naming.

Verification gaps:

- No `enable_testing()` / `add_test()` entry points.
- Existing contract scripts are string checks, not compiled or runtime tests.
- Visual regression exists but is manual and GPU/runtime dependent.
- No focused tests for OBJ/USD loader edge cases.
- No tests for Hydra instancer index bounds or dome light quaternion conversion.

Recommended first verification work:

1. Make all Python scripts explicit UTF-8 readers.
2. Update stale contracts to match current LiDAR composite architecture.
3. Add CTest wrappers for contract scripts.
4. Add small CPU-only tests for loaders and data conversion helpers.

## Potential Bugs Worth Prioritizing

### Dome Light Quaternion

`hdRobot/light.cpp:238` uses:

```cpp
rotateQuat.GetImaginary()[3]
```

`GetImaginary()` is xyz. The quaternion w component should likely come from
`GetReal()`. This is a likely out-of-bounds bug.

### Instancer Index Bounds

`hdRobot/instancer.cpp:277` checks:

```cpp
if(i < translations.size())
```

but accesses:

```cpp
translations[instanceIndex]
```

The same pattern applies to rotations, scales, and instance transforms. If
instance indices are non-contiguous or reordered, this can go out of bounds.

### Implicit Sphere BLAS

`headless/hello_vulkan_mesh.cpp:88` always appends `sphereToVkGeometryKHR()` to
BLAS inputs. Some paths may not have created sphere buffers. Confirm whether
empty implicit geometry is valid. If not, guard this path.

### BLAS/TLAS Rebuild Accumulation

`createBottomLevelAS()` and `createTopLevelAS()` reserve/append but do not clear
`m_blas` and `m_tlas`. Repeated calls can accumulate stale acceleration inputs.

### Manual Instance Append

`ray_trace_app.cpp:388` pushes directly into `m_helloVk.m_instances` without
using a helper and without updating `m_instanceIds`. This bypasses invariants.

### Vulkan Result Checks

Several calls do not check return values:

- `vkCreatePipelineLayout`
- `vkCreateRayTracingPipelinesKHR`
- `vkCreateComputePipelines`
- descriptor allocation/updates in some paths

Add a `VK_CHECK`-style helper before refactoring pipeline code.

### USD Loader Risks

`demo/usd_loader.cpp` has multiple suspected issues:

- Multi-mesh index accumulation can map global indices into current mesh points.
- UV access can index past available texcoords.
- `for(auto v : m_vertices)` modifies copies, so V flip is ineffective.

### OBJ Loader Error Handling

`common/obj_loader.cpp` ignores `ParseFromFile` return value and relies on
`assert(reader.Valid())`. Release builds can continue after invalid input.

## Recommended Refactor Plan

### Phase 0 - Safety Net

Scope: scripts, CTest, small CPU-only checks.

Tasks:

- Fix Python script encoding.
- Update stale DLSS/LiDAR contract checks.
- Add CTest entry points.
- Add loader conversion tests for empty/malformed OBJ, material index bounds,
  USD multi-mesh, missing UVs if feasible.

Do this before touching render architecture.

### Phase 1 - Readability Cleanup

Scope: no behavior changes.

Tasks:

- Expand formatting script to include `common`, `demo`, and `hdRobot`.
- Fix corrupted comments near touched code.
- Remove stale commented-out code blocks.
- Normalize obvious local variable names in touched files.
- Replace ad hoc debug prints with the appropriate logging mechanism.

Keep formatting-only changes in separate commits.

### Phase 2 - Low-Risk Module Extraction

Scope: behavior-preserving file splits.

Tasks:

- Extract `hdRobot` scene data from `renderParam.h`.
- Extract settings conversion helpers from `renderPass.cpp`.
- Extract texture resolving/exporting from `renderPass.cpp`.
- Extract render target descriptor refresh helper in `HelloVulkan`.
- Extract shared image barrier helper used by pipeline and LiDAR files.

These changes should be mostly mechanical and easy to review.

### Phase 3 - State Encapsulation

Scope: introduce narrow APIs, migrate call sites.

Tasks:

- Add `HelloVulkan::addInstance()` or equivalent.
- Prevent direct external mutation of `m_instances` where practical.
- Add `HdRobotRenderParam` mutation/access methods.
- Move dirty flag writes behind named methods.
- Split `ModelLoader` into a plain `ModelData` container plus loader interfaces
  if callers actually need polymorphism.

### Phase 4 - Renderer Architecture

Scope: higher-risk decoupling.

Tasks:

- Add `HeadlessRenderBridge` between Hydra render pass and `RayTraceApp`.
- Extract `RenderTargets` from `HelloVulkan`.
- Extract `RayTracingPass` and `LidarPass`.
- Convert `RayTraceApp::render()` from an implicit method chain into an explicit
  pass sequence with clear inputs/outputs.
- Make DLSS and LiDAR feature gates easier to reason about.

## Suggested First Issue

Start with Phase 0:

1. Fix Python contract scripts so they run on Windows and match current code.
2. Add CTest targets for those scripts.
3. Commit that as the refactor safety net.

This gives future refactors a stable baseline and will also clarify which
existing checks are intentionally obsolete.
