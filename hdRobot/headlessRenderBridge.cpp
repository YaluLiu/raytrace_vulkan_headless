#include "headlessRenderBridge.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <utility>

#include "camera.h"
#include "renderBuffer.h"
#include "renderTextureExport.h"
#include "sceneData.h"
#include "tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
WaveFrontMaterial ToWaveFrontMaterial(const HydraMaterial &material)
{
  WaveFrontMaterial result;
  result.ambient = material.ambient;
  result.diffuse = material.diffuse;
  result.specular = material.specular;
  result.transmittance = material.transmittance;
  result.emission = material.emission;
  result.baseColorFactor = material.baseColorFactor;
  result.emissionFactor = material.emissionFactor;
  result.transmissionColorFactor = material.transmissionColorFactor;
  result.subsurfaceColorFactor = material.subsurfaceColorFactor;
  result.shininess = material.shininess;
  result.ior = material.ior;
  result.opaque = material.opaque;
  result.metallicFactor = material.metallicFactor;
  result.roughnessFactor = material.roughnessFactor;
  result.opacityFactor = material.opacityFactor;
  result.transmissionFactor = material.transmissionFactor;
  result.subsurfaceFactor = material.subsurfaceFactor;
  result.subsurfaceScale = material.subsurfaceScale;
  result.illum = material.illum;
  result.diffuseTextureId = material.diffuseTextureId;
  result.baseColorTextureId = material.baseColorTextureId;
  result.metallicTextureId = material.metallicTextureId;
  result.roughnessTextureId = material.roughnessTextureId;
  result.normalTextureId = material.normalTextureId;
  result.emissionTextureId = material.emissionTextureId;
  result.opacityTextureId = material.opacityTextureId;
  result.subsurfaceTextureId = material.subsurfaceTextureId;
  return result;
}

HeadlessCameraData ToHeadlessCameraData(const HdRobotCameraData& camera)
{
  HeadlessCameraData result;
  result.name      = camera.name;
  result.position  = camera.position;
  result.forward   = camera.forward;
  result.up        = camera.up;
  result.vfov_deg  = camera.vfov_deg;
  result.clipStart = camera.clipStart;
  result.clipEnd   = camera.clipEnd;
  return result;
}

std::vector<HeadlessCameraData> ToHeadlessCameraData(const std::vector<HdRobotCameraData>& cameras)
{
  std::vector<HeadlessCameraData> result;
  result.reserve(cameras.size());
  for(const HdRobotCameraData& camera : cameras)
  {
    result.push_back(ToHeadlessCameraData(camera));
  }
  return result;
}

HeadlessTileConfig ToHeadlessTileConfig(const HdRobotTileConfig& config)
{
  HeadlessTileConfig result;
  result.enabled      = config.enabled;
  result.cameraWidth  = config.cameraWidth;
  result.cameraHeight = config.cameraHeight;
  result.gridColumns  = config.gridColumns;
  result.gridRows     = config.gridRows;
  result.sanitize();
  return result;
}

TfTokenVector NormalizeRenderTags(TfTokenVector tags)
{
  std::sort(tags.begin(), tags.end(), [](const TfToken &a, const TfToken &b) { return a.GetString() < b.GetString(); });
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

bool RenderTagsEqual(const TfTokenVector &lhs, const TfTokenVector &rhs)
{
  if (lhs.size() != rhs.size())
  {
    return false;
  }
  for (size_t i = 0; i < lhs.size(); ++i)
  {
    if (lhs[i] != rhs[i])
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

bool HasRenderBuffer(const HdRenderPassAovBindingVector &bindings)
{
  return std::any_of(bindings.begin(), bindings.end(), [](const HdRenderPassAovBinding &binding) {
    return binding.renderBuffer != nullptr;
  });
}

std::optional<GfVec2i> GetRenderBufferSize(const HdRenderPassAovBindingVector &bindings)
{
  for (const HdRenderPassAovBinding &binding : bindings)
  {
    if (binding.renderBuffer == nullptr || IsFixedSizeTileAov(binding.aovName))
    {
      continue;
    }

    auto *renderBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    const int width = static_cast<int>(renderBuffer->GetWidth());
    const int height = static_cast<int>(renderBuffer->GetHeight());
    if (width > 0 && height > 0)
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
  if (framing.IsValid())
  {
    const GfVec2i size = framing.dataWindow.GetSize();
    if (size[0] > 0 && size[1] > 0)
    {
      return size;
    }
  }

  const GfVec4f &viewport = renderPassState->GetViewport();
  const int viewportWidth = static_cast<int>(viewport[2]);
  const int viewportHeight = static_cast<int>(viewport[3]);
  if (viewportWidth > 0 && viewportHeight > 0)
  {
    return GfVec2i(viewportWidth, viewportHeight);
  }

  return GetRenderBufferSize(bindings);
}

}  // namespace

HeadlessRenderBridge::HeadlessRenderBridge(HdRobotRenderParam &renderParam, std::string resourcePath)
    : _renderParam(renderParam), _resourcePath(std::move(resourcePath))
{
}

HeadlessRenderBridge::~HeadlessRenderBridge()
{
  _glInteropCache.Clear();
}

bool HeadlessRenderBridge::RenderFrame(const HdRenderPassStateSharedPtr &renderPassState,
                                       const TfTokenVector &renderTags)
{
  if (updateActiveRenderTags(renderTags))
  {
    _renderParam.MarkAllMeshesInstanceDirty();
  }

  if (!renderPassState)
  {
    return false;
  }

  const auto &hdAovBindings = renderPassState->GetAovBindings();
  if (!HasRenderBuffer(hdAovBindings))
  {
    return false;
  }

  const std::optional<GfVec2i> mainRenderSize = GetMainRenderSize(renderPassState, hdAovBindings);
  if (!mainRenderSize)
  {
    return false;
  }

  ensureHeadlessReady(*mainRenderSize);
  if (!updateCameras(renderPassState))
  {
    return false;
  }

  _renderApp.getVulkan().setTileConfig(ToHeadlessTileConfig(_renderParam.GetTileConfig()));
  updateLights();
  updateScene();
  _renderApp.render();

  const ::HelloVulkan &app = _renderApp.getVulkan();
  bool allAovsCopied = true;
  for (const HdRenderPassAovBinding &binding : hdAovBindings)
  {
    auto *aovBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    const bool copied = CopyAovToRenderBuffer(app, binding.aovName, aovBuffer, _glInteropCache);
    allAovsCopied = allAovsCopied && copied;
    if (aovBuffer != nullptr)
    {
      aovBuffer->SetConverged(copied);
    }
  }

  return allAovsCopied;
}

void HeadlessRenderBridge::ensureHeadlessReady(const GfVec2i& renderSize)
{
  if (!_isAppInited)
  {
    initializeHeadless(renderSize);
  }
  else if (_renderSize != renderSize)
  {
    resizeHeadless(renderSize);
  }

  refreshTextureAssetsIfNeeded();
}

void HeadlessRenderBridge::initializeHeadless(const GfVec2i& renderSize)
{
  if (!_resourcePath.empty())
  {
    _renderApp.setPluginSearchRoot(std::filesystem::path(_resourcePath).parent_path().string());
  }

  _renderApp.setup(renderSize[0], renderSize[1]);
  _renderSize = renderSize;
  _isAppInited = true;

  HelloVulkan &vulkan = _renderApp.getVulkan();
  vulkan.loadTextureAssets(ExportRegisteredTextures(_renderParam.GetTextureAssets()));
  _uploadedTextureRegistryVersion = _renderParam.GetTextureRegistryVersion();
  uploadInitialScene();
  _renderApp.createRenderResources();
}

void HeadlessRenderBridge::resizeHeadless(const GfVec2i& renderSize)
{
  _glInteropCache.Clear();
  _renderApp.resize(renderSize[0], renderSize[1]);
  _renderSize = renderSize;
}

void HeadlessRenderBridge::uploadInitialScene()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    ModelLoader loader;
    ConvertVmeshToLoader(curMesh, loader);
    loader.m_textures.clear();
    for (auto &matId : curMesh.scene_mat_ids)
    {
      auto materialObj = _renderParam.v_mat[matId].toMaterialObj();
      loader.m_materials.emplace_back(materialObj);
    }
    vulkan.loadModel(loader);
    const auto instance = vulkan.getInstance(vulkan.getInstanceCount() - 1);
    const size_t firstInstanceId = vulkan.getInstanceCount() - 1;
    const size_t authoredInstanceCount = curMesh.instanceTransforms.size();
    const size_t rendererInstanceSlotCount = std::max<size_t>(authoredInstanceCount, 1);
    curMesh.hasInstances = authoredInstanceCount > 0;
    for (size_t i = 1; i < rendererInstanceSlotCount; ++i)
    {
      vulkan.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
    }
    curMesh.rendererInstanceIds.clear();
    for (size_t i = 0; i < rendererInstanceSlotCount; ++i)
    {
      curMesh.rendererInstanceIds.push_back(static_cast<int>(i + firstInstanceId));
    }
    _renderParam.MarkMeshInstanceDirty(meshId);
  }
}

void HeadlessRenderBridge::refreshTextureAssetsIfNeeded()
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
  HelloVulkan& vulkan = _renderApp.getVulkan();
  vulkan.recreateTextureResources(ExportRegisteredTextures(_renderParam.GetTextureAssets()));
  _uploadedTextureRegistryVersion = textureRegistryVersion;
}

bool HeadlessRenderBridge::updateCameras(const HdRenderPassStateSharedPtr &renderPassState)
{
  const HdCamera *hdCamera = renderPassState ? renderPassState->GetCamera() : nullptr;
  if (!hdCamera)
  {
    return false;
  }

  const HdRobotCamera *robotCamera = dynamic_cast<const HdRobotCamera *>(hdCamera);
  if (robotCamera == nullptr)
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

  HelloVulkan& vulkan = _renderApp.getVulkan();
  vulkan.setCameras(ToHeadlessCameraData(cameras));
  vulkan.setMainCamera(ToHeadlessCameraData(mainCameraData));
  return true;
}

void HeadlessRenderBridge::updateLights()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  vulkan.clearLights();

  for (auto &curLight : _renderParam.v_light)
  {
    if (curLight.valid)
    {
      vulkan.addLight(curLight.toLight());
    }
  }
}

void HeadlessRenderBridge::updateScene()
{
  updateGeometry();
  updateInstances();
  updateMaterials();
}

void HeadlessRenderBridge::updateGeometry()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    if (_renderParam.ConsumeMeshGeometryDirty(meshId))
    {
      ConvertVmeshToLoader(_renderParam.v_mesh[meshId], vulkan.m_Loader[meshId]);
      vulkan.updateMeshGeometry(meshId);
    }
  }
}

void HeadlessRenderBridge::updateInstances()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();

  for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    if (_renderParam.ConsumeMeshInstanceDirty(meshId))
    {
      const bool tagMatched = isMeshRenderTagMatched(curMesh);
      const bool meshVisible = curMesh.visible && curMesh.valid && tagMatched;
      for (size_t instanceId = 0; instanceId < curMesh.rendererInstanceIds.size(); ++instanceId)
      {
        auto rendererInstanceId = curMesh.rendererInstanceIds[instanceId];
        const bool instanceValid = curMesh.hasInstances && (instanceId < curMesh.instanceTransforms.size());
        const bool visible = meshVisible && instanceValid;
        glm::mat4 transform = glm::transpose(curMesh.transform);
        if (instanceValid)
        {
          transform = glm::transpose(curMesh.transform * curMesh.instanceTransforms[instanceId]);
        }
        vulkan.updateInstance(rendererInstanceId, transform, visible);
      }
    }
  }
}

void HeadlessRenderBridge::updateMaterials()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  std::vector<MaterialUpdate> newMaterials;
  for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    for (size_t localMatIdx = 0; localMatIdx < curMesh.scene_mat_ids.size(); ++localMatIdx)
    {
      int globalMatId = curMesh.scene_mat_ids[localMatIdx];
      auto &material = _renderParam.v_mat[globalMatId];
      if (_renderParam.IsMaterialDirty(static_cast<size_t>(globalMatId)))
      {
        WaveFrontMaterial newMaterial = ToWaveFrontMaterial(material);
        newMaterials.push_back({static_cast<int>(meshId), static_cast<int>(localMatIdx), newMaterial});
      }
    }
  }
  if (!newMaterials.empty())
  {
    vulkan.updateMaterialsAtRuntime(newMaterials);
  }
  _renderParam.ClearAllMaterialDirty();
}

bool HeadlessRenderBridge::updateActiveRenderTags(const TfTokenVector &renderTags)
{
  TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
  if (RenderTagsEqual(_activeRenderTags, normalizedRenderTags))
  {
    return false;
  }

  _activeRenderTags = std::move(normalizedRenderTags);
  return true;
}

bool HeadlessRenderBridge::isMeshRenderTagMatched(const HydraMesh &mesh) const
{
  if (_activeRenderTags.empty())
  {
    return true;
  }
  return std::find(_activeRenderTags.begin(), _activeRenderTags.end(), mesh.renderTag) != _activeRenderTags.end();
}

PXR_NAMESPACE_CLOSE_SCOPE
