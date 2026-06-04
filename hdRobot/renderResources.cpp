#include "renderResources.h"

#include "backendScene.h"
#include "camera.h"
#include "glInteropCache.h"
#include "hydraTextureAssetExport.h"
#include "renderBridgeConversions.h"
#include "renderParam.h"
#include "tokens.h"

#include <engine/engine.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
GfVec2i BootstrapRenderSize()
{
  return GfVec2i(1, 1);
}

bool IsUsdImagingPluginCamera(const HdRobotCameraData& camera)
{
  constexpr const char* kInternalCameraPrefix = "/_UsdImaging_HdRobotRendererPlugin_";
  constexpr const char* kInternalCameraSuffix = "/camera";
  return camera.name.starts_with(kInternalCameraPrefix) && camera.name.ends_with(kInternalCameraSuffix);
}

TfTokenVector NormalizeRenderTags(TfTokenVector tags)
{
  std::sort(tags.begin(), tags.end(), [](const TfToken& a, const TfToken& b) { return a.GetString() < b.GetString(); });
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

bool RenderTagsEqual(const TfTokenVector& lhs, const TfTokenVector& rhs)
{
  if(lhs.size() != rhs.size())
  {
    return false;
  }
  for(size_t i = 0; i < lhs.size(); ++i)
  {
    if(lhs[i] != rhs[i])
    {
      return false;
    }
  }
  return true;
}

bool IsMeshRenderTagMatched(const HydraMesh& mesh, const TfTokenVector& renderTags)
{
  if(renderTags.empty())
  {
    return true;
  }
  return std::find(renderTags.begin(), renderTags.end(), mesh.renderTag) != renderTags.end();
}

void SortCamerasByName(std::vector<HdRobotCameraData>& cameras)
{
  std::sort(cameras.begin(), cameras.end(), [](const HdRobotCameraData& lhs, const HdRobotCameraData& rhs) {
    return lhs.name < rhs.name;
  });
}

void SortLidarSensorsByName(std::vector<HdRobotLidarSensorData>& sensors)
{
  std::sort(sensors.begin(), sensors.end(), [](const HdRobotLidarSensorData& lhs,
                                               const HdRobotLidarSensorData& rhs) {
    return lhs.name < rhs.name;
  });
}

void SortHeightScanSensorsByName(std::vector<HdRobotHeightScanSensorData>& sensors)
{
  std::sort(sensors.begin(), sensors.end(), [](const HdRobotHeightScanSensorData& lhs,
                                               const HdRobotHeightScanSensorData& rhs) {
    return lhs.name < rhs.name;
  });
}

std::vector<Material> CollectMaterialsForMesh(const HydraMesh& mesh,
                                              const std::vector<HdRobotMaterialRecord>& materials)
{
  std::vector<Material> materialsForUpload;
  materialsForUpload.reserve(mesh.scene_mat_ids.size());
  for(const int matId : mesh.scene_mat_ids)
  {
    const size_t materialIndex = matId >= 0 ? static_cast<size_t>(matId) : 0;
    const size_t fallbackIndex = materials.empty() ? materialIndex : 0;
    const size_t resolvedIndex = materialIndex < materials.size() ? materialIndex : fallbackIndex;
    if(resolvedIndex < materials.size())
    {
      materialsForUpload.emplace_back(ToMaterial(materials[resolvedIndex].data));
    }
  }
  if(materialsForUpload.empty())
  {
    HydraMaterial fallback;
    fallback.set_default();
    materialsForUpload.emplace_back(ToMaterial(fallback));
  }
  return materialsForUpload;
}

struct UploadedMeshState
{
  int rendererMeshId = -1;
  std::vector<int> rendererInstanceIds;
};

UploadedMeshState UploadMeshToRenderer(::Engine& engine,
                                       const HydraMesh& mesh,
                                       const std::vector<HdRobotMaterialRecord>& materials)
{
  UploadedMeshState state;
  state.rendererMeshId = static_cast<int>(engine.getMeshSourceCount());

  MeshGeometry geometry;
  ConvertHydraMeshToGeometry(mesh, geometry);
  std::vector<Material> materialsForUpload = CollectMaterialsForMesh(mesh, materials);
  engine.uploadMesh(geometry, materialsForUpload);

  const size_t firstInstanceId = engine.getInstanceCount() - 1;
  const auto instance = engine.getInstance(firstInstanceId);
  const size_t authoredInstanceCount = mesh.instanceTransforms.size();
  const size_t rendererInstanceSlotCount = std::max<size_t>(authoredInstanceCount, 1);
  for(size_t i = 1; i < rendererInstanceSlotCount; ++i)
  {
    engine.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
  }
  state.rendererInstanceIds.reserve(rendererInstanceSlotCount);
  for(size_t i = 0; i < rendererInstanceSlotCount; ++i)
  {
    state.rendererInstanceIds.push_back(static_cast<int>(i + firstInstanceId));
  }
  return state;
}

bool EnsureMeshRendererInstanceSlots(::Engine& engine, HdRobotMeshRecord& record)
{
  if(record.rendererInstanceIds.empty())
  {
    return false;
  }

  const size_t requiredSlotCount = std::max<size_t>(record.data.instanceTransforms.size(), 1);
  if(record.rendererInstanceIds.size() >= requiredSlotCount)
  {
    return false;
  }

  const InstanceInfo baseInstance = engine.getInstance(static_cast<size_t>(record.rendererInstanceIds.front()));
  for(size_t i = record.rendererInstanceIds.size(); i < requiredSlotCount; ++i)
  {
    const uint32_t rendererInstanceId = engine.addInstance(baseInstance.transform, baseInstance.objIndex, static_cast<int>(i));
    record.rendererInstanceIds.push_back(static_cast<int>(rendererInstanceId));
  }
  return true;
}
} // namespace

struct HdRobotRenderResources::Impl
{
  explicit Impl(std::string resourcePath)
      : resourcePath(std::move(resourcePath))
  {
  }

  ~Impl()
  {
    glInteropCache.Clear();
  }

  void EnsureEngineReady(const GfVec2i& renderSize)
  {
    if(!isAppInited)
    {
      InitializeEngine(renderSize);
    }
    else if(currentRenderSize != renderSize)
    {
      ResizeEngine(renderSize);
    }
  }

  void CommitResources(HdRobotRenderParam& renderParam, HdRobotBackendScene& backendScene)
  {
    EnsureEngineReady(isAppInited ? currentRenderSize : BootstrapRenderSize());
    EnsureInitialResources(backendScene);
    RefreshTextureAssetsIfNeeded(backendScene);

    CacheCommittedFrameState(renderParam, backendScene);
    SubmitCommittedCameras();
    ConfigureFrameOutputs(requestedTileAovChannels);

    UpdateLights(backendScene);
    UpdateGeometry(backendScene);
    if(UpdateInstances(backendScene))
    {
      frameVisibilityDirty = true;
    }
    UpdateMaterials(backendScene);

    frameMeshRecords = backendScene.GetMeshRecordsSnapshot();
  }

  void ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels)
  {
    requestedTileAovChannels = requestedTileChannels;
    engine.configureOutputs(ToRendererOutputConfig(tileConfig, requestedTileAovChannels, lidarSensors,
                                                   lidarVisualizationConfig, heightScanSensors,
                                                   heightScanVisualizationConfig));
  }

  bool SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState)
  {
    const HdCamera* hdCamera = renderPassState ? renderPassState->GetCamera() : nullptr;
    if(!hdCamera)
    {
      return false;
    }

    const HdRobotCamera* robotCamera = dynamic_cast<const HdRobotCamera*>(hdCamera);
    if(robotCamera == nullptr)
    {
      return false;
    }

    HdRobotCameraData mainCameraData = robotCamera->GetCameraData();
    const CameraConformPolicy frameConformPolicy = ToCameraConformPolicy(renderPassState->GetWindowPolicy());
    mainCameraData.conformPolicy = frameConformPolicy;
    std::vector<HdRobotCameraData> cameras = camerasSnapshot;
    if(cameras.empty())
    {
      cameras.push_back(mainCameraData);
    }
    else
    {
      const auto cameraIt = std::find_if(cameras.begin(), cameras.end(), [&mainCameraData](const HdRobotCameraData& camera) {
        return camera.name == mainCameraData.name;
      });
      if(cameraIt == cameras.end())
      {
        cameras.push_back(mainCameraData);
      }
      else
      {
        *cameraIt = mainCameraData;
      }
    }

    cameras.erase(std::remove_if(cameras.begin(), cameras.end(), IsUsdImagingPluginCamera), cameras.end());
    for(HdRobotCameraData& camera : cameras)
    {
      camera.conformPolicy = frameConformPolicy;
    }
    SortCamerasByName(cameras);

    engine.setCameras(ToCameraSpec(cameras));
    engine.setMainCamera(ToCameraSpec(mainCameraData));
    return true;
  }

  void ApplyFrameRenderTags(const TfTokenVector& renderTags)
  {
    TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
    const bool tagsChanged = !RenderTagsEqual(activeRenderTags, normalizedRenderTags);
    if(!tagsChanged && !frameVisibilityDirty)
    {
      return;
    }

    activeRenderTags = std::move(normalizedRenderTags);
    for(const HdRobotMeshRecord& record : frameMeshRecords)
    {
      const HydraMesh& mesh = record.data;
      const bool tagMatched = IsMeshRenderTagMatched(mesh, activeRenderTags);
      const bool meshVisible = record.active && mesh.visible && mesh.valid && tagMatched;
      const uint32_t traceMask = mesh.traceRole == HdRobotTraceRoleTokens->ground
                                     ? kTraceMaskGround
                                     : kTraceMaskDefaultGeometry;

      for(size_t instanceId = 0; instanceId < record.rendererInstanceIds.size(); ++instanceId)
      {
        const int rendererInstanceId = record.rendererInstanceIds[instanceId];
        if(rendererInstanceId < 0)
        {
          continue;
        }

        const bool instanceValid = mesh.hasInstances && (instanceId < mesh.instanceTransforms.size());
        const bool visible = meshVisible && instanceValid;
        glm::mat4 transform = glm::transpose(mesh.transform);
        if(instanceValid)
        {
          transform = glm::transpose(mesh.transform * mesh.instanceTransforms[instanceId]);
        }
        engine.updateInstance(static_cast<uint32_t>(rendererInstanceId), transform, visible, traceMask);
      }
    }
    frameVisibilityDirty = false;
  }

  void InitializeEngine(const GfVec2i& renderSize)
  {
    if(!resourcePath.empty())
    {
      engine.setPluginSearchRoot(std::filesystem::path(resourcePath).parent_path().string());
    }

    engine.setup(renderSize[0], renderSize[1]);
    currentRenderSize = renderSize;
    isAppInited = true;
  }

  void ResizeEngine(const GfVec2i& renderSize)
  {
    glInteropCache.Clear();
    engine.resize(renderSize[0], renderSize[1]);
    currentRenderSize = renderSize;
  }

  void EnsureInitialResources(HdRobotBackendScene& backendScene)
  {
    if(renderResourcesCreated)
    {
      return;
    }

    engine.loadTextureAssets(ExportRegisteredTextures(backendScene.GetTextureAssetsSnapshot()));
    uploadedTextureRegistryVersion = backendScene.GetTextureRegistryVersion();
    UploadInitialScene(backendScene);
    engine.createRenderResources();
    renderResourcesCreated = true;
    frameVisibilityDirty = true;
  }

  void UploadInitialScene(HdRobotBackendScene& backendScene)
  {
    std::vector<HdRobotMeshRecord> meshRecords = backendScene.GetMeshRecordsSnapshot();
    const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
    for(const HdRobotMeshRecord& record : meshRecords)
    {
      if(!record.active || !record.data.valid)
      {
        continue;
      }
      UploadedMeshState state = UploadMeshToRenderer(engine, record.data, materialRecords);
      backendScene.SetMeshRendererState(record.backendIndex, state.rendererMeshId, std::move(state.rendererInstanceIds));
    }
  }

  void RefreshTextureAssetsIfNeeded(HdRobotBackendScene& backendScene)
  {
    const uint64_t textureRegistryVersion = backendScene.GetTextureRegistryVersion();
    if(textureRegistryVersion == uploadedTextureRegistryVersion)
    {
      return;
    }

    glInteropCache.Clear();
    engine.rebuildTextureResourcesAndSceneBindings(ExportRegisteredTextures(backendScene.GetTextureAssetsSnapshot()));
    uploadedTextureRegistryVersion = textureRegistryVersion;
  }

  void CacheCommittedFrameState(HdRobotRenderParam& renderParam, const HdRobotBackendScene& backendScene)
  {
    camerasSnapshot = backendScene.GetActiveCameraSnapshot();
    SortCamerasByName(camerasSnapshot);

    lidarSensors = backendScene.GetActiveLidarSensorSnapshot();
    SortLidarSensorsByName(lidarSensors);

    heightScanSensors = backendScene.GetActiveHeightScanSensorSnapshot();
    SortHeightScanSensorsByName(heightScanSensors);

    tileConfig = renderParam.GetTileConfig();
    lidarVisualizationConfig = renderParam.GetLidarVisualizationConfig();
    heightScanVisualizationConfig = renderParam.GetHeightScanVisualizationConfig();
  }

  void SubmitCommittedCameras()
  {
    std::vector<HdRobotCameraData> cameras = camerasSnapshot;
    cameras.erase(std::remove_if(cameras.begin(), cameras.end(), IsUsdImagingPluginCamera), cameras.end());
    SortCamerasByName(cameras);
    engine.setCameras(ToCameraSpec(cameras));
  }

  void UpdateLights(const HdRobotBackendScene& backendScene)
  {
    engine.clearLights();

    for(const HydraLight& curLight : backendScene.GetActiveLightSnapshot())
    {
      engine.addLight(ToLight(curLight));
    }
  }

  void UpdateGeometry(HdRobotBackendScene& backendScene)
  {
    const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
    for(const HdRobotMeshRecord& record : backendScene.ConsumeDirtyMeshGeometry())
    {
      if(!record.active || !record.data.valid)
      {
        continue;
      }
      if(record.rendererMeshId < 0)
      {
        UploadedMeshState state = UploadMeshToRenderer(engine, record.data, materialRecords);
        backendScene.SetMeshRendererState(record.backendIndex, state.rendererMeshId, std::move(state.rendererInstanceIds));
        continue;
      }
      MeshGeometry geometry;
      ConvertHydraMeshToGeometry(record.data, geometry);
      engine.updateMeshGeometry(static_cast<uint32_t>(record.rendererMeshId), geometry);
    }
  }

  bool UpdateInstances(HdRobotBackendScene& backendScene)
  {
    bool updatedAnyInstance = false;
    const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();

    for(HdRobotMeshRecord record : backendScene.ConsumeDirtyMeshInstances())
    {
      updatedAnyInstance = true;
      if(record.rendererMeshId < 0 && record.active && record.data.valid)
      {
        UploadedMeshState state = UploadMeshToRenderer(engine, record.data, materialRecords);
        record.rendererMeshId = state.rendererMeshId;
        record.rendererInstanceIds = std::move(state.rendererInstanceIds);
        backendScene.SetMeshRendererState(record.backendIndex, record.rendererMeshId, record.rendererInstanceIds);
      }
      if(EnsureMeshRendererInstanceSlots(engine, record))
      {
        backendScene.SetMeshRendererState(record.backendIndex, record.rendererMeshId, record.rendererInstanceIds);
      }

      const HydraMesh& mesh = record.data;
      const bool meshVisible = record.active && mesh.visible && mesh.valid;
      const uint32_t traceMask = mesh.traceRole == HdRobotTraceRoleTokens->ground
                                     ? kTraceMaskGround
                                     : kTraceMaskDefaultGeometry;
      for(size_t instanceId = 0; instanceId < record.rendererInstanceIds.size(); ++instanceId)
      {
        const int rendererInstanceId = record.rendererInstanceIds[instanceId];
        if(rendererInstanceId < 0)
        {
          continue;
        }

        const bool instanceValid = mesh.hasInstances && (instanceId < mesh.instanceTransforms.size());
        const bool visible = meshVisible && instanceValid;
        glm::mat4 transform = glm::transpose(mesh.transform);
        if(instanceValid)
        {
          transform = glm::transpose(mesh.transform * mesh.instanceTransforms[instanceId]);
        }
        engine.updateInstance(static_cast<uint32_t>(rendererInstanceId), transform, visible, traceMask);
      }
    }
    return updatedAnyInstance;
  }

  void UpdateMaterials(HdRobotBackendScene& backendScene)
  {
    const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
    const std::vector<HdRobotMeshRecord> meshRecords = backendScene.GetMeshRecordsSnapshot();
    const std::vector<uint32_t> dirtyMaterialIndices = backendScene.ConsumeDirtyMaterialIndices();
    const std::set<uint32_t> dirtyMaterialSet(dirtyMaterialIndices.begin(), dirtyMaterialIndices.end());
    std::vector<MaterialUpdate> newMaterials;
    for(const HdRobotMeshRecord& record : meshRecords)
    {
      if(record.rendererMeshId < 0)
      {
        continue;
      }
      const HydraMesh& curMesh = record.data;
      for(size_t localMatIdx = 0; localMatIdx < curMesh.scene_mat_ids.size(); ++localMatIdx)
      {
        int globalMatId = curMesh.scene_mat_ids[localMatIdx];
        if(globalMatId < 0 || static_cast<size_t>(globalMatId) >= materialRecords.size())
        {
          globalMatId = 0;
        }
        if(dirtyMaterialSet.find(static_cast<uint32_t>(globalMatId)) != dirtyMaterialSet.end())
        {
          Material newMaterial = ToMaterial(materialRecords[static_cast<size_t>(globalMatId)].data);
          newMaterials.push_back({record.rendererMeshId, static_cast<int>(localMatIdx), newMaterial});
        }
      }
    }
    if(!newMaterials.empty())
    {
      engine.updateMaterialsAtRuntime(newMaterials);
    }
  }

  std::string resourcePath;
  ::Engine engine;
  ::HdRobotGlInteropCache glInteropCache;
  bool isAppInited = false;
  bool renderResourcesCreated = false;
  GfVec2i currentRenderSize{-1, -1};
  uint64_t uploadedTextureRegistryVersion = 0;

  std::vector<HdRobotCameraData> camerasSnapshot;
  std::vector<HdRobotLidarSensorData> lidarSensors;
  std::vector<HdRobotHeightScanSensorData> heightScanSensors;
  HdRobotTileConfig tileConfig;
  HdRobotLidarVisualizationConfig lidarVisualizationConfig;
  HdRobotHeightScanVisualizationConfig heightScanVisualizationConfig;
  TileAovChannelMask requestedTileAovChannels = TileAovChannelMask::ColorDepth();

  TfTokenVector activeRenderTags;
  std::vector<HdRobotMeshRecord> frameMeshRecords;
  bool frameVisibilityDirty = true;
};

HdRobotRenderResources::HdRobotRenderResources(std::string resourcePath)
    : _impl(std::make_unique<Impl>(std::move(resourcePath)))
{
}

HdRobotRenderResources::~HdRobotRenderResources() = default;

::Engine& HdRobotRenderResources::GetEngine()
{
  return _impl->engine;
}

const ::Engine& HdRobotRenderResources::GetEngine() const
{
  return _impl->engine;
}

::HdRobotGlInteropCache& HdRobotRenderResources::GetGlInteropCache()
{
  return _impl->glInteropCache;
}

void HdRobotRenderResources::EnsureEngineReady(const GfVec2i& renderSize)
{
  _impl->EnsureEngineReady(renderSize);
}

void HdRobotRenderResources::CommitResources(HdRobotRenderParam& renderParam, HdRobotBackendScene& backendScene)
{
  _impl->CommitResources(renderParam, backendScene);
}

bool HdRobotRenderResources::SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState)
{
  return _impl->SetFrameCamera(renderPassState);
}

void HdRobotRenderResources::ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels)
{
  _impl->ConfigureFrameOutputs(requestedTileChannels);
}

void HdRobotRenderResources::ApplyFrameRenderTags(const TfTokenVector& renderTags)
{
  _impl->ApplyFrameRenderTags(renderTags);
}

PXR_NAMESPACE_CLOSE_SCOPE
