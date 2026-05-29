#include "rasterBridge.h"

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
#include "hydraRasterAovCopy.h"
#include "hydraTextureAssetExport.h"
#include "rasterBridgeConversions.h"
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

std::vector<RasterMaterial> CollectRasterMaterialsForMesh(const HydraMesh &mesh,
                                                          const std::vector<HdRobotMaterialRecord> &materials)
{
  std::vector<RasterMaterial> rasterMaterials;
  rasterMaterials.reserve(mesh.scene_mat_ids.size());
  for(const int matId : mesh.scene_mat_ids)
  {
    const size_t materialIndex = matId >= 0 ? static_cast<size_t>(matId) : 0;
    const size_t fallbackIndex = materials.empty() ? materialIndex : 0;
    const size_t resolvedIndex = materialIndex < materials.size() ? materialIndex : fallbackIndex;
    if(resolvedIndex < materials.size())
    {
      rasterMaterials.emplace_back(ToRasterMaterial(materials[resolvedIndex].data));
    }
  }
  if(rasterMaterials.empty())
  {
    HydraMaterial fallback;
    fallback.set_default();
    rasterMaterials.emplace_back(ToRasterMaterial(fallback));
  }
  return rasterMaterials;
}

struct UploadedMeshState
{
  int rendererMeshId = -1;
  std::vector<int> rendererInstanceIds;
};

UploadedMeshState UploadMeshToRenderer(RasterRenderer &vulkan,
                                       const HydraMesh &mesh,
                                       const std::vector<HdRobotMaterialRecord> &materials)
{
  UploadedMeshState state;
  state.rendererMeshId = static_cast<int>(vulkan.getMeshSourceCount());

  RasterMeshGeometry geometry;
  ConvertHydraMeshToRasterGeometry(mesh, geometry);
  std::vector<RasterMaterial> rasterMaterials = CollectRasterMaterialsForMesh(mesh, materials);
  vulkan.uploadMesh(geometry, rasterMaterials);

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

bool EnsureMeshRendererInstanceSlots(RasterRenderer &vulkan, HdRobotMeshRecord &record)
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

  const RasterInstanceInfo baseInstance = vulkan.getInstance(static_cast<size_t>(record.rendererInstanceIds.front()));
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

HdRobotRasterBridge::HdRobotRasterBridge(HdRobotRenderParam &renderParam, std::string resourcePath)
    : _renderParam(renderParam), _resourcePath(std::move(resourcePath))
{
}

HdRobotRasterBridge::~HdRobotRasterBridge()
{
  _glInteropCache.Clear();
}

bool HdRobotRasterBridge::RenderRasterFrame(const HdRenderPassStateSharedPtr &renderPassState,
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

  ensureRasterSessionReady(*mainRenderSize);
  if(!updateCameras(renderPassState))
  {
    return false;
  }

  _rasterSession.getRenderer().setTileConfig(ToTileAtlasConfig(_renderParam.GetTileConfig()));
  _rasterSession.getRenderer().setRequestedTileAovChannels(ComputeHdRobotRequestedTileAovChannels(hdAovBindings));
  const HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  std::vector<HdRobotLidarSensorData> lidarSensors = backendScene.GetActiveLidarSensorSnapshot();
  std::sort(lidarSensors.begin(), lidarSensors.end(), [](const HdRobotLidarSensorData &lhs,
                                                         const HdRobotLidarSensorData &rhs) {
    return lhs.name < rhs.name;
  });
  _rasterSession.getRenderer().setLidarSensors(ToRasterLidarSensorSpec(lidarSensors));
  std::vector<HdRobotHeightScanSensorData> heightScanSensors = backendScene.GetActiveHeightScanSensorSnapshot();
  std::sort(heightScanSensors.begin(), heightScanSensors.end(), [](const HdRobotHeightScanSensorData &lhs,
                                                                   const HdRobotHeightScanSensorData &rhs) {
    return lhs.name < rhs.name;
  });
  _rasterSession.getRenderer().setHeightScanSensors(ToRasterHeightScanSensorSpec(heightScanSensors));
  _rasterSession.getRenderer().setHeightScanVisualizationConfig(
      ToRasterHeightScanVisualizationConfig(_renderParam.GetHeightScanVisualizationConfig()));
  _rasterSession.getRenderer().setLidarVisualizationConfig(
      ToRasterLidarVisualizationConfig(_renderParam.GetLidarVisualizationConfig()));
  updateLights();
  updateScene();
  _rasterSession.render();

  const ::RasterRenderer &app = _rasterSession.getRenderer();
  bool allAovsCopied = true;

  auto copyBinding = [&](const HdRenderPassAovBinding &binding)
  {
    auto *aovBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    bool copied = false;
    const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(binding.aovName);
    if(spec)
    {
      const HdRobotAovCopyRequest request{binding.aovName, spec->rasterAov, spec->copyScaling, aovBuffer};
      copied = CopyAovToRenderBuffer(app, request, _glInteropCache);
    }
    else if(IsHdRobotIgnoredAov(binding.aovName))
    {
      copied = true;
    }
    else
    {
      std::cerr << "[HdRobotRasterBridge] Unsupported AOV token " << binding.aovName.GetString() << std::endl;
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

void HdRobotRasterBridge::ensureRasterSessionReady(const GfVec2i &renderSize)
{
  if(!_isAppInited)
  {
    initializeRasterSession(renderSize);
  }
  else if(_renderSize != renderSize)
  {
    resizeRasterSession(renderSize);
  }

  refreshTextureAssetsIfNeeded();
}

void HdRobotRasterBridge::initializeRasterSession(const GfVec2i &renderSize)
{
  if(!_resourcePath.empty())
  {
    _rasterSession.setPluginSearchRoot(std::filesystem::path(_resourcePath).parent_path().string());
  }

  _rasterSession.setup(renderSize[0], renderSize[1]);
  _renderSize = renderSize;
  _isAppInited = true;

  RasterRenderer &vulkan = _rasterSession.getRenderer();
  vulkan.loadTextureAssets(ExportRegisteredTextures(_renderParam.GetTextureAssets()));
  _uploadedTextureRegistryVersion = _renderParam.GetTextureRegistryVersion();
  uploadInitialScene();
  _rasterSession.createRenderResources();
}

void HdRobotRasterBridge::resizeRasterSession(const GfVec2i &renderSize)
{
  _glInteropCache.Clear();
  _rasterSession.resize(renderSize[0], renderSize[1]);
  _renderSize = renderSize;
}

void HdRobotRasterBridge::uploadInitialScene()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  std::vector<HdRobotMeshRecord> meshRecords = backendScene.GetMeshRecordsSnapshot();
  const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
  for(const HdRobotMeshRecord& record : meshRecords)
  {
    if(!record.active || !record.data.valid)
    {
      continue;
    }
    UploadedMeshState state = UploadMeshToRenderer(vulkan, record.data, materialRecords);
    backendScene.SetMeshRendererState(record.backendIndex, state.rendererMeshId, std::move(state.rendererInstanceIds));
  }
}

void HdRobotRasterBridge::refreshTextureAssetsIfNeeded()
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
  RasterRenderer &vulkan = _rasterSession.getRenderer();
  vulkan.rebuildTextureResourcesAndSceneBindings(ExportRegisteredTextures(_renderParam.GetTextureAssets()));
  _uploadedTextureRegistryVersion = textureRegistryVersion;
}

bool HdRobotRasterBridge::updateCameras(const HdRenderPassStateSharedPtr &renderPassState)
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

  RasterRenderer &vulkan = _rasterSession.getRenderer();
  vulkan.setCameras(ToRasterCameraSpec(cameras));
  vulkan.setMainCamera(ToRasterCameraSpec(mainCameraData));
  return true;
}

void HdRobotRasterBridge::updateLights()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();
  vulkan.clearLights();

  for(const HydraLight &curLight : _renderParam.GetBackendScene().GetActiveLightSnapshot())
  {
    vulkan.addLight(ToRasterLight(curLight));
  }
}

void HdRobotRasterBridge::updateScene()
{
  updateGeometry();
  updateInstances();
  updateMaterials();
}

void HdRobotRasterBridge::updateGeometry()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();
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
      UploadedMeshState state = UploadMeshToRenderer(vulkan, record.data, materialRecords);
      backendScene.SetMeshRendererState(record.backendIndex, state.rendererMeshId, std::move(state.rendererInstanceIds));
      continue;
    }
    RasterMeshGeometry geometry;
    ConvertHydraMeshToRasterGeometry(record.data, geometry);
    vulkan.updateMeshGeometry(static_cast<uint32_t>(record.rendererMeshId), geometry);
  }
}

void HdRobotRasterBridge::updateInstances()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();

  for(HdRobotMeshRecord record : backendScene.ConsumeDirtyMeshInstances())
  {
    if(record.rendererMeshId < 0 && record.active && record.data.valid)
    {
      UploadedMeshState state = UploadMeshToRenderer(vulkan, record.data, materialRecords);
      record.rendererMeshId = state.rendererMeshId;
      record.rendererInstanceIds = std::move(state.rendererInstanceIds);
      backendScene.SetMeshRendererState(record.backendIndex, record.rendererMeshId, record.rendererInstanceIds);
    }
    if(EnsureMeshRendererInstanceSlots(vulkan, record))
    {
      backendScene.SetMeshRendererState(record.backendIndex, record.rendererMeshId, record.rendererInstanceIds);
    }

    const HydraMesh& curMesh = record.data;
    const bool tagMatched = isMeshRenderTagMatched(curMesh);
    const bool meshVisible = record.active && curMesh.visible && curMesh.valid && tagMatched;
    const uint32_t traceMask = curMesh.traceRole == HdRobotTraceRoleTokens->ground
                                 ? kRasterTraceMaskGround
                                 : kRasterTraceMaskDefaultGeometry;
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
      vulkan.updateInstance(rendererInstanceId, transform, visible, traceMask);
    }
  }
}

void HdRobotRasterBridge::updateMaterials()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();
  HdRobotBackendScene& backendScene = _renderParam.GetBackendScene();
  const std::vector<HdRobotMaterialRecord> materialRecords = backendScene.GetMaterialRecordsSnapshot();
  const std::vector<HdRobotMeshRecord> meshRecords = backendScene.GetMeshRecordsSnapshot();
  const std::vector<uint32_t> dirtyMaterialIndices = backendScene.ConsumeDirtyMaterialIndices();
  const std::set<uint32_t> dirtyMaterialSet(dirtyMaterialIndices.begin(), dirtyMaterialIndices.end());
  std::vector<RasterMaterialUpdate> newMaterials;
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
        RasterMaterial newMaterial = ToRasterMaterial(materialRecords[static_cast<size_t>(globalMatId)].data);
        newMaterials.push_back({record.rendererMeshId, static_cast<int>(localMatIdx), newMaterial});
      }
    }
  }
  if(!newMaterials.empty())
  {
    vulkan.updateMaterialsAtRuntime(newMaterials);
  }
}

bool HdRobotRasterBridge::updateActiveRenderTags(const TfTokenVector &renderTags)
{
  TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
  if(RenderTagsEqual(_activeRenderTags, normalizedRenderTags))
  {
    return false;
  }

  _activeRenderTags = std::move(normalizedRenderTags);
  return true;
}

bool HdRobotRasterBridge::isMeshRenderTagMatched(const HydraMesh &mesh) const
{
  if(_activeRenderTags.empty())
  {
    return true;
  }
  return std::find(_activeRenderTags.begin(), _activeRenderTags.end(), mesh.renderTag) != _activeRenderTags.end();
}

PXR_NAMESPACE_CLOSE_SCOPE
