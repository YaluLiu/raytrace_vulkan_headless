#include "renderBridge.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "aovBridgeSpec.h"
#include "camera.h"
#include "hydraAovCopy.h"
#include "hydraTextureAssetExport.h"
#include "renderBridgeConversions.h"
#include "renderBuffer.h"
#include "sceneData.h"
#include "tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
bool IsUsdImagingPluginCamera(const HdRobotCameraData &camera)
{
  constexpr const char *kInternalCameraPrefix = "/_UsdImaging_HdRobotRendererPlugin_";
  constexpr const char *kInternalCameraSuffix = "/camera";
  return camera.name.starts_with(kInternalCameraPrefix) && camera.name.ends_with(kInternalCameraSuffix);
}

TfTokenVector NormalizeRenderTags(TfTokenVector tags)
{
  std::sort(tags.begin(), tags.end(), [](const TfToken &a, const TfToken &b) { return a.GetString() < b.GetString(); });
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

bool RenderTagsEqual(const TfTokenVector &lhs, const TfTokenVector &rhs)
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

std::vector<Material> CollectMaterialsForMesh(const HydraMesh &mesh,
                                                          const std::vector<HdRobotMaterialRecord> &materials)
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

UploadedMeshState UploadMeshToRenderer(Engine &vulkan,
                                       const HydraMesh &mesh,
                                       const std::vector<HdRobotMaterialRecord> &materials)
{
  UploadedMeshState state;
  state.rendererMeshId = static_cast<int>(vulkan.getMeshSourceCount());

  MeshGeometry geometry;
  ConvertHydraMeshToGeometry(mesh, geometry);
  std::vector<Material> materialsForUpload = CollectMaterialsForMesh(mesh, materials);
  vulkan.uploadMesh(geometry, materialsForUpload);

  const size_t firstInstanceId = vulkan.getInstanceCount() - 1;
  const auto instance = vulkan.getInstance(firstInstanceId);
  const size_t authoredInstanceCount = mesh.instanceTransforms.size();
  const size_t rendererInstanceSlotCount = std::max<size_t>(authoredInstanceCount, 1);
  for(size_t i = 1; i < rendererInstanceSlotCount; ++i)
  {
    vulkan.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
  }
  state.rendererInstanceIds.reserve(rendererInstanceSlotCount);
  for(size_t i = 0; i < rendererInstanceSlotCount; ++i)
  {
    state.rendererInstanceIds.push_back(static_cast<int>(i + firstInstanceId));
  }
  return state;
}

bool EnsureMeshRendererInstanceSlots(Engine &vulkan, HdRobotMeshRecord &record)
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

  const InstanceInfo baseInstance = vulkan.getInstance(static_cast<size_t>(record.rendererInstanceIds.front()));
  for(size_t i = record.rendererInstanceIds.size(); i < requiredSlotCount; ++i)
  {
    const uint32_t rendererInstanceId = vulkan.addInstance(baseInstance.transform, baseInstance.objIndex, static_cast<int>(i));
    record.rendererInstanceIds.push_back(static_cast<int>(rendererInstanceId));
  }
  return true;
}

bool HasRenderBuffer(const HdRenderPassAovBindingVector &bindings)
{
  return std::any_of(bindings.begin(), bindings.end(),
                     [](const HdRenderPassAovBinding &binding) { return binding.renderBuffer != nullptr; });
}

std::optional<GfVec2i> GetRenderBufferSize(const HdRenderPassAovBindingVector &bindings)
{
  for(const HdRenderPassAovBinding &binding : bindings)
  {
    if(binding.renderBuffer == nullptr || IsHdRobotFixedSizeTileAov(binding.aovName))
    {
      continue;
    }

    auto *renderBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    const int width = static_cast<int>(renderBuffer->GetWidth());
    const int height = static_cast<int>(renderBuffer->GetHeight());
    if(width > 0 && height > 0)
    {
      return GfVec2i(width, height);
    }
  }

  return std::nullopt;
}

std::optional<GfVec2i> GetMainRenderSize(const HdRenderPassStateSharedPtr &renderPassState,
                                         const HdRenderPassAovBindingVector &bindings)
{
  const CameraUtilFraming &framing = renderPassState->GetFraming();
  if(framing.IsValid())
  {
    const GfVec2i size = framing.dataWindow.GetSize();
    if(size[0] > 0 && size[1] > 0)
    {
      return size;
    }
  }

  const GfVec4f &viewport = renderPassState->GetViewport();
  const int viewportWidth = static_cast<int>(viewport[2]);
  const int viewportHeight = static_cast<int>(viewport[3]);
  if(viewportWidth > 0 && viewportHeight > 0)
  {
    return GfVec2i(viewportWidth, viewportHeight);
  }

  return GetRenderBufferSize(bindings);
}

} // namespace

HdRobotRenderBridge::HdRobotRenderBridge(HdRobotRenderParam &renderParam, std::string resourcePath)
    : _renderParam(renderParam), _resourcePath(std::move(resourcePath))
{
}

HdRobotRenderBridge::~HdRobotRenderBridge()
{
  _glInteropCache.Clear();
}

bool HdRobotRenderBridge::RenderFrame(const HdRenderPassStateSharedPtr &renderPassState,
                                       const TfTokenVector &renderTags)
{
  if(updateActiveRenderTags(renderTags))
  {
    _renderParam.MarkAllMeshesInstanceDirty();
  }

  if(!renderPassState)
  {
    return false;
  }

  const auto &hdAovBindings = renderPassState->GetAovBindings();
  if(!HasRenderBuffer(hdAovBindings))
  {
    return false;
  }

  const std::optional<GfVec2i> mainRenderSize = GetMainRenderSize(renderPassState, hdAovBindings);
  if(!mainRenderSize)
  {
    return false;
  }

  ensureEngineReady(*mainRenderSize);
  if(!updateCameras(renderPassState))
  {
    return false;
  }

  configureRendererOutputs(hdAovBindings);
  commitRendererResources();
  _engine.render();

  return copyRenderedAovs(hdAovBindings);
}

bool HdRobotRenderBridge::copyRenderedAovs(const HdRenderPassAovBindingVector &hdAovBindings)
{
  bool allAovsCopied = true;

  auto copyBinding = [&](const HdRenderPassAovBinding &binding)
  {
    auto *aovBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    bool copied = false;
    const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(binding.aovName);
    if(spec)
    {
      const HdRobotAovCopyRequest request{binding.aovName, spec->engineAov, spec->copyScaling, aovBuffer};
      copied = CopyAovToRenderBuffer(_engine, request, _glInteropCache);
    }
    else if(IsHdRobotIgnoredAov(binding.aovName))
    {
      copied = true;
    }
    else
    {
      std::cerr << "[HdRobotRenderBridge] Unsupported AOV token " << binding.aovName.GetString() << std::endl;
    }
    allAovsCopied = allAovsCopied && copied;
    if(aovBuffer != nullptr)
    {
      aovBuffer->SetConverged(copied);
    }
  };

  std::vector<const HdRenderPassAovBinding *> copyOrder;
  copyOrder.reserve(hdAovBindings.size());
  for(const HdRenderPassAovBinding &binding : hdAovBindings)
  {
    copyOrder.push_back(&binding);
  }
  std::stable_sort(copyOrder.begin(), copyOrder.end(), [](const HdRenderPassAovBinding *lhs,
                                                          const HdRenderPassAovBinding *rhs) {
    return GetHdRobotAovCopyPriority(lhs->aovName) < GetHdRobotAovCopyPriority(rhs->aovName);
  });
  for(const HdRenderPassAovBinding *binding : copyOrder)
  {
    copyBinding(*binding);
  }

  return allAovsCopied;
}

void HdRobotRenderBridge::ensureEngineReady(const GfVec2i &renderSize)
{
  if(!_isAppInited)
  {
    initializeEngine(renderSize);
  }
  else if(_renderSize != renderSize)
  {
    resizeEngine(renderSize);
  }
}

void HdRobotRenderBridge::initializeEngine(const GfVec2i &renderSize)
{
  if(!_resourcePath.empty())
  {
    _engine.setPluginSearchRoot(std::filesystem::path(_resourcePath).parent_path().string());
  }

  _engine.setup(renderSize[0], renderSize[1]);
  _renderSize = renderSize;
  _isAppInited = true;

  _engine.loadTextureAssets(ExportRegisteredTextures(_renderParam.GetTextureAssets()));
  _uploadedTextureRegistryVersion = _renderParam.GetTextureRegistryVersion();
  uploadInitialScene();
  _engine.createRenderResources();
}

void HdRobotRenderBridge::resizeEngine(const GfVec2i &renderSize)
{
  _glInteropCache.Clear();
  _engine.resize(renderSize[0], renderSize[1]);
  _renderSize = renderSize;
}

void HdRobotRenderBridge::commitRendererResources()
{
  refreshTextureAssetsIfNeeded();
  updateLights();
  updateGeometry();
  updateInstances();
  updateMaterials();
}

void HdRobotRenderBridge::uploadInitialScene()
{
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  std::vector<HdRobotMeshRecord> meshRecords = backendScene.GetMeshRecordsSnapshot();
  const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
  for(const HdRobotMeshRecord& record : meshRecords)
  {
    if(!record.active || !record.data.valid)
    {
      continue;
    }
    UploadedMeshState state = UploadMeshToRenderer(_engine, record.data, materialRecords);
    backendScene.SetMeshRendererState(record.backendIndex, state.rendererMeshId, std::move(state.rendererInstanceIds));
  }
}

void HdRobotRenderBridge::refreshTextureAssetsIfNeeded()
{
  if(!_isAppInited)
  {
    return;
  }

  const uint64_t textureRegistryVersion = _renderParam.GetTextureRegistryVersion();
  if(textureRegistryVersion == _uploadedTextureRegistryVersion)
  {
    return;
  }

  _glInteropCache.Clear();
  _engine.rebuildTextureResourcesAndSceneBindings(ExportRegisteredTextures(_renderParam.GetTextureAssets()));
  _uploadedTextureRegistryVersion = textureRegistryVersion;
}

bool HdRobotRenderBridge::updateCameras(const HdRenderPassStateSharedPtr &renderPassState)
{
  const HdCamera *hdCamera = renderPassState ? renderPassState->GetCamera() : nullptr;
  if(!hdCamera)
  {
    return false;
  }

  const HdRobotCamera *robotCamera = dynamic_cast<const HdRobotCamera *>(hdCamera);
  if(robotCamera == nullptr)
  {
    return false;
  }

  const HdRobotCameraData &mainCameraData = robotCamera->GetCameraData();
  std::vector<HdRobotCameraData> cameras = _renderParam.GetBackendScene().GetActiveCameraSnapshot();
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
  std::sort(cameras.begin(), cameras.end(), [](const HdRobotCameraData &lhs, const HdRobotCameraData &rhs) {
    return lhs.name < rhs.name;
  });

  _engine.setCameras(ToCameraSpec(cameras));
  _engine.setMainCamera(ToCameraSpec(mainCameraData));
  return true;
}

void HdRobotRenderBridge::configureRendererOutputs(const HdRenderPassAovBindingVector &hdAovBindings)
{
  const HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  std::vector<HdRobotLidarSensorData> lidarSensors = backendScene.GetActiveLidarSensorSnapshot();
  std::sort(lidarSensors.begin(), lidarSensors.end(), [](const HdRobotLidarSensorData &lhs,
                                                         const HdRobotLidarSensorData &rhs) {
    return lhs.name < rhs.name;
  });
  std::vector<HdRobotHeightScanSensorData> heightScanSensors = backendScene.GetActiveHeightScanSensorSnapshot();
  std::sort(heightScanSensors.begin(), heightScanSensors.end(), [](const HdRobotHeightScanSensorData &lhs,
                                                                   const HdRobotHeightScanSensorData &rhs) {
    return lhs.name < rhs.name;
  });
  _engine.configureOutputs(ToRendererOutputConfig(
      _renderParam.GetTileConfig(), ComputeHdRobotRequestedTileAovChannels(hdAovBindings), lidarSensors,
      _renderParam.GetLidarVisualizationConfig(), heightScanSensors,
      _renderParam.GetHeightScanVisualizationConfig()));
}

void HdRobotRenderBridge::updateLights()
{
  _engine.clearLights();

  for(const HydraLight &curLight : _renderParam.GetBackendScene().GetActiveLightSnapshot())
  {
    _engine.addLight(ToLight(curLight));
  }
}

void HdRobotRenderBridge::updateGeometry()
{
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
  for(const HdRobotMeshRecord& record : backendScene.ConsumeDirtyMeshGeometry())
  {
    if(!record.active || !record.data.valid)
    {
      continue;
    }
    if(record.rendererMeshId < 0)
    {
      UploadedMeshState state = UploadMeshToRenderer(_engine, record.data, materialRecords);
      backendScene.SetMeshRendererState(record.backendIndex, state.rendererMeshId, std::move(state.rendererInstanceIds));
      continue;
    }
    MeshGeometry geometry;
    ConvertHydraMeshToGeometry(record.data, geometry);
    _engine.updateMeshGeometry(static_cast<uint32_t>(record.rendererMeshId), geometry);
  }
}

void HdRobotRenderBridge::updateInstances()
{
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();

  for(HdRobotMeshRecord record : backendScene.ConsumeDirtyMeshInstances())
  {
    if(record.rendererMeshId < 0 && record.active && record.data.valid)
    {
      UploadedMeshState state = UploadMeshToRenderer(_engine, record.data, materialRecords);
      record.rendererMeshId = state.rendererMeshId;
      record.rendererInstanceIds = std::move(state.rendererInstanceIds);
      backendScene.SetMeshRendererState(record.backendIndex, record.rendererMeshId, record.rendererInstanceIds);
    }
    if(EnsureMeshRendererInstanceSlots(_engine, record))
    {
      backendScene.SetMeshRendererState(record.backendIndex, record.rendererMeshId, record.rendererInstanceIds);
    }

    const HydraMesh& curMesh = record.data;
    const bool tagMatched = isMeshRenderTagMatched(curMesh);
    const bool meshVisible = record.active && curMesh.visible && curMesh.valid && tagMatched;
    const uint32_t traceMask = curMesh.traceRole == HdRobotTraceRoleTokens->ground
                                 ? kTraceMaskGround
                                 : kTraceMaskDefaultGeometry;
    for(size_t instanceId = 0; instanceId < record.rendererInstanceIds.size(); ++instanceId)
    {
      auto rendererInstanceId = record.rendererInstanceIds[instanceId];
      const bool instanceValid = curMesh.hasInstances && (instanceId < curMesh.instanceTransforms.size());
      const bool visible = meshVisible && instanceValid;
      glm::mat4 transform = glm::transpose(curMesh.transform);
      if(instanceValid)
      {
        transform = glm::transpose(curMesh.transform * curMesh.instanceTransforms[instanceId]);
      }
      _engine.updateInstance(rendererInstanceId, transform, visible, traceMask);
    }
  }
}

void HdRobotRenderBridge::updateMaterials()
{
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
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
    _engine.updateMaterialsAtRuntime(newMaterials);
  }
}

bool HdRobotRenderBridge::updateActiveRenderTags(const TfTokenVector &renderTags)
{
  TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
  if(RenderTagsEqual(_activeRenderTags, normalizedRenderTags))
  {
    return false;
  }

  _activeRenderTags = std::move(normalizedRenderTags);
  return true;
}

bool HdRobotRenderBridge::isMeshRenderTagMatched(const HydraMesh &mesh) const
{
  if(_activeRenderTags.empty())
  {
    return true;
  }
  return std::find(_activeRenderTags.begin(), _activeRenderTags.end(), mesh.renderTag) != _activeRenderTags.end();
}

PXR_NAMESPACE_CLOSE_SCOPE
