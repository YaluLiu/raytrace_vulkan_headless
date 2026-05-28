#include "rasterBridge.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

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

bool IsFixedSizeTileAov(const TfToken &name)
{
  return name == HdRobotAovTokens->tileColor || name == HdRobotAovTokens->tileDepth;
}

bool IsDisplayTileAov(const TfToken &name)
{
  return name == HdRobotAovTokens->tileDisplayColor || name == HdRobotAovTokens->tileDisplayDepth;
}

TileAovChannelMask ComputeRequestedTileAovChannels(const HdRenderPassAovBindingVector &bindings)
{
  TileAovChannelMask channels = TileAovChannelMask::None();
  for(const HdRenderPassAovBinding &binding : bindings)
  {
    if(binding.renderBuffer == nullptr)
    {
      continue;
    }

    if(binding.aovName == HdRobotAovTokens->tileColor || binding.aovName == HdRobotAovTokens->tileDisplayColor)
    {
      channels |= TileAovChannelMask::FromChannel(TileAovChannel::Color);
    }
    else if(binding.aovName == HdRobotAovTokens->tileDepth || binding.aovName == HdRobotAovTokens->tileDisplayDepth)
    {
      channels |= TileAovChannelMask::FromChannel(TileAovChannel::Depth);
    }
  }
  return channels;
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
    if(binding.renderBuffer == nullptr || IsFixedSizeTileAov(binding.aovName))
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
  _rasterSession.getRenderer().setRequestedTileAovChannels(ComputeRequestedTileAovChannels(hdAovBindings));
  std::vector<HdRobotLidarSensorData> lidarSensors = _renderParam.GetLidarSensorsSnapshot();
  std::sort(lidarSensors.begin(), lidarSensors.end(), [](const HdRobotLidarSensorData &lhs,
                                                         const HdRobotLidarSensorData &rhs) {
    return lhs.name < rhs.name;
  });
  _rasterSession.getRenderer().setLidarSensors(ToRasterLidarSensorSpec(lidarSensors));
  std::vector<HdRobotHeightScanSensorData> heightScanSensors = _renderParam.GetHeightScanSensorsSnapshot();
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
    const bool copied = CopyAovToRenderBuffer(app, binding.aovName, aovBuffer, _glInteropCache);
    allAovsCopied = allAovsCopied && copied;
    if(aovBuffer != nullptr)
    {
      aovBuffer->SetConverged(copied);
    }
  };

  auto copyMatchingBindings = [&](const auto &predicate)
  {
    for(const HdRenderPassAovBinding &binding : hdAovBindings)
    {
      if(predicate(binding.aovName))
      {
        copyBinding(binding);
      }
    }
  };

  copyMatchingBindings(IsFixedSizeTileAov);
  copyMatchingBindings(IsDisplayTileAov);
  copyMatchingBindings([](const TfToken &name) { return !IsFixedSizeTileAov(name) && !IsDisplayTileAov(name); });

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
  for(size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    ModelLoader loader;
    ConvertVmeshToLoader(curMesh, loader);
    loader.m_textures.clear();
    for(auto &matId : curMesh.scene_mat_ids)
    {
      auto materialObj = _renderParam.v_mat[matId].toMaterialObj();
      loader.m_materials.emplace_back(materialObj);
    }
    vulkan.uploadMeshFromLoader(loader);
    const auto instance = vulkan.getInstance(vulkan.getInstanceCount() - 1);
    const size_t firstInstanceId = vulkan.getInstanceCount() - 1;
    const size_t authoredInstanceCount = curMesh.instanceTransforms.size();
    const size_t rendererInstanceSlotCount = std::max<size_t>(authoredInstanceCount, 1);
    curMesh.hasInstances = authoredInstanceCount > 0;
    for(size_t i = 1; i < rendererInstanceSlotCount; ++i)
    {
      vulkan.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
    }
    curMesh.rendererInstanceIds.clear();
    for(size_t i = 0; i < rendererInstanceSlotCount; ++i)
    {
      curMesh.rendererInstanceIds.push_back(static_cast<int>(i + firstInstanceId));
    }
    _renderParam.MarkMeshInstanceDirty(meshId);
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
  _renderParam.UpsertCamera(mainCameraData);

  std::vector<HdRobotCameraData> cameras = _renderParam.GetCamerasSnapshot();
  if(cameras.empty())
  {
    cameras.push_back(mainCameraData);
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

  for(auto &curLight : _renderParam.v_light)
  {
    if(curLight.valid)
    {
      vulkan.addLight(curLight.toLight());
    }
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
  for(size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    if(_renderParam.ConsumeMeshGeometryDirty(meshId))
    {
      ConvertVmeshToLoader(_renderParam.v_mesh[meshId], vulkan.getMutableMeshSourceLoader(meshId));
      vulkan.updateMeshGeometry(meshId);
    }
  }
}

void HdRobotRasterBridge::updateInstances()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();

  for(size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    if(_renderParam.ConsumeMeshInstanceDirty(meshId))
    {
      const bool tagMatched = isMeshRenderTagMatched(curMesh);
      const bool meshVisible = curMesh.visible && curMesh.valid && tagMatched;
      const uint32_t traceMask = curMesh.traceRole == HdRobotTraceRoleTokens->ground
                                   ? kRasterTraceMaskGround
                                   : kRasterTraceMaskDefaultGeometry;
      for(size_t instanceId = 0; instanceId < curMesh.rendererInstanceIds.size(); ++instanceId)
      {
        auto rendererInstanceId = curMesh.rendererInstanceIds[instanceId];
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
}

void HdRobotRasterBridge::updateMaterials()
{
  RasterRenderer &vulkan = _rasterSession.getRenderer();
  std::vector<RasterMaterialUpdate> newMaterials;
  for(size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    for(size_t localMatIdx = 0; localMatIdx < curMesh.scene_mat_ids.size(); ++localMatIdx)
    {
      int globalMatId = curMesh.scene_mat_ids[localMatIdx];
      auto &material = _renderParam.v_mat[globalMatId];
      if(_renderParam.IsMaterialDirty(static_cast<size_t>(globalMatId)))
      {
        WaveFrontMaterial newMaterial = ToWaveFrontMaterial(material);
        newMaterials.push_back({static_cast<int>(meshId), static_cast<int>(localMatIdx), newMaterial});
      }
    }
  }
  if(!newMaterials.empty())
  {
    vulkan.updateMaterialsAtRuntime(newMaterials);
  }
  _renderParam.ClearAllMaterialDirty();
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
