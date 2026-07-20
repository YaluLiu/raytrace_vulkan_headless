# hdRobot Domain Map

Read this map for Hydra delegate work under `hdRobot/`. Schema and imaging
adapter details are routed separately through `STABLE_INTEGRATIONS.md`.

## Render Flow And Ownership

| Files | Responsibility |
| --- | --- |
| `hdRobot/rendererPlugin.*` | Hydra renderer-plugin registration. |
| `hdRobot/renderDelegate.*` | Primitive support, render settings, scene/session ownership, commit, and commands. |
| `hdRobot/renderPass.*` | Hydra render-pass entry into the pass bridge. |
| `hdRobot/passBridge.*` | Validates pass state, configures cameras/outputs, renders, and orders AOV copies. |
| `hdRobot/engineSession.*` | Owns `Engine`, render size, committed snapshots, GL interop, and capture state. |
| `hdRobot/engineSceneSync.*` | Converts pending scene-store changes into initial and incremental engine updates. |
| `hdRobot/passBridgeConversions.*` | Hydra-side camera, mesh, material, light, sensor, visualization, and tile conversions. |

The main runtime path is `HdRenderPass::_Execute` →
`PassBridge::RenderFrameAndCopyAovs` → `EngineSession` → public `Engine` calls.
Scene mutations enter separately through `RenderDelegate::CommitResources` →
`EngineSceneSync`.

## CPU Scene State

| Files | Responsibility |
| --- | --- |
| `hdRobot/backendHandles.h` | Stable generation handles for backend records. |
| `hdRobot/slotVector.h` | Generation-checked handle storage. |
| `hdRobot/sceneStore.*` | CPU records, texture registry, active snapshots, and pending/dirty update queues. |
| `hdRobot/renderParam.*` | Render settings only; it does not own scene-store state. |
| `hdRobot/sceneData.*` | Shared Hydra record and conversion helpers. |
| `hdRobot/tokens.*` | Delegate settings, AOV tokens, trace roles, and compatibility names. |

Renderer mesh/instance bindings belong to `EngineSceneSync`, not `SceneStore`.

## Hydra Primitives

| Files | Responsibility |
| --- | --- |
| `hdRobot/mesh.*` | Topology/primvars, tangents, subsets, instancing, visibility, trace role, and mesh updates. |
| `hdRobot/material.*` | Material sync, network parsing, texture registration, and backend updates. |
| `hdRobot/materialXParser.*` | UsdPreviewSurface/MaterialX surface selection and upstream texture/primvar traversal. |
| `hdRobot/camera.*` | Camera pose/projection sync and shared camera-data conversion. |
| `hdRobot/lidarSensor.*` | Hydra LiDAR sprim state, validation, and scene-store updates. |
| `hdRobot/heightScanSensor.*` | Hydra height-scan sprim state, pose derivation, validation, and updates. |
| `hdRobot/light.*` | Hydra light conversion and queued updates. |
| `hdRobot/instancer.*` | Hydra instancer transforms. |

## AOV Export And Interop

| Files | Responsibility |
| --- | --- |
| `hdRobot/aovBridgeSpec.*` | Canonical Hydra-token-to-engine-AOV mapping, descriptors, tile requests, and copy order. |
| `hdRobot/renderBuffer.*` | Hydra buffer allocation, Hgi texture descriptors, and format conversion. |
| `hdRobot/hydraAovCopy.*` | Engine AOV to Hydra render-buffer copy, including tile scaling policy. |
| `hdRobot/glInteropCache.*` | Imported OpenGL texture cache and stale-import eviction. |
| `hdRobot/hydraTextureAssetExport.*` | Encoded material texture export into engine texture assets. |
| `hdRobot/lidarPointCloudCsv.*` | Delegate-command LiDAR CSV serialization and sensor filtering. |

## Focused Test

- `hdRobot/tests/lidar_point_cloud_csv_test.cpp`

## Question Routes

| Topic | Start with | Search symbols |
| --- | --- | --- |
| Hydra render frame | `renderPass.cpp`, `passBridge.cpp`, `engineSession.cpp` | `_Execute`, `RenderFrameAndCopyAovs`, `EnsureEngineReady` |
| Pending scene updates | `renderDelegate.cpp`, `engineSceneSync.cpp`, `sceneStore.cpp` | `CommitResources`, `ApplyPendingUpdates` |
| Mesh sync | `mesh.cpp`, `engineSceneSync.cpp` | `_CreateGiMeshes`, `_AnalyzePrimvars`, `updateGeometry` |
| Cameras/sensors | `camera.cpp`, sensor sprims, `passBridgeConversions.cpp` | `EnqueueCameraUpdate`, `GetActiveCameraSnapshot` |
| AOV mapping/copy | `aovBridgeSpec.cpp`, `hydraAovCopy.cpp`, `renderBuffer.cpp` | `GetHdRobotAovSpec`, `CopyAovToRenderBuffer` |
| MaterialX parsing | `materialXParser.cpp`, `material.cpp` | `ParseMaterialXNetwork`, `SelectSurfaceShaderCandidate` |
| Texture export | `hydraTextureAssetExport.cpp`, `glInteropCache.cpp` | `ExportRegisteredTextures`, `GetTexturePath` |
| Instancers/subsets | `instancer.cpp`, `mesh.cpp`, `sceneData.cpp` | `ComputeFlattenedTransforms`, `GetGeomSubsets` |
| Light conversion | `light.cpp`, `sceneData.cpp`, `passBridgeConversions.cpp` | `Sync`, `ToLight`, `updateLights` |
