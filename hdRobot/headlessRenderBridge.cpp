#include "headlessRenderBridge.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "camera.h"
#include "nvh/cameramanipulator.hpp"
#include "renderBuffer.h"
#include "renderSettings.h"
#include "renderTextureExport.h"
#include "sceneData.h"

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

RayTraceApp::SensorCameraInput ToHeightScanCameraInput(const HdRobotHeightScanData &heightScanData)
{
  RayTraceApp::SensorCameraInput camera{};
  camera.eye    = heightScanData.eye;
  camera.center = heightScanData.center;
  camera.up     = heightScanData.up;
  camera.fovDeg = heightScanData.fovDeg;
  return camera;
}
}  // namespace

HeadlessRenderBridge::HeadlessRenderBridge(const HdRenderSettingsMap &settings, HdRobotRenderParam &renderParam,
                                           std::string resourcePath)
    : _settings(settings), _renderParam(renderParam), _resourcePath(std::move(resourcePath))
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
    _renderParam.MarkAllMeshesTlasDirty();
  }

  if (!renderPassState)
  {
    return false;
  }

  const auto &hdAovBindings = renderPassState->GetAovBindings();
  HdRobotRenderBuffer *primaryRenderBuffer = GetPrimaryRenderBuffer(hdAovBindings);
  if (primaryRenderBuffer == nullptr)
  {
    return false;
  }

  if (_width != static_cast<int>(primaryRenderBuffer->GetWidth()) ||
      _height != static_cast<int>(primaryRenderBuffer->GetHeight()))
  {
    _width = static_cast<int>(primaryRenderBuffer->GetWidth());
    _height = static_cast<int>(primaryRenderBuffer->GetHeight());
    _resetRenderBuffer = true;
  }

  applyRenderSettings();
  initOrResize();
  if (!updateMainCamera(renderPassState))
  {
    return false;
  }

  updateLidarCamera();
  updateHeightScanCamera();
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

  ++_frameIndex;
  return allAovsCopied;
}

void HeadlessRenderBridge::applyRenderSettings()
{
  HelloVulkan &app = _renderApp.getVulkan();
  if (_isAppInited && RenderSettingsMayReallocateAovs(_settings, app))
  {
    _glInteropCache.Clear();
  }
  ApplyRenderSettingsToApp(_settings, app);
}

void HeadlessRenderBridge::initOrResize()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();

  if (!_isAppInited)
  {
    _isAppInited = true;
    if (!_resourcePath.empty())
    {
      _renderApp.setPluginSearchRoot(std::filesystem::path(_resourcePath).parent_path().string());
    }
    _renderApp.setup(_width, _height);
    _resetRenderBuffer = false;

    bool isFirstModel = true;
    for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
    {
      auto &curMesh = _renderParam.v_mesh[meshId];
      ModelLoader loader;
      ConvertVmeshToLoader(curMesh, loader);
      loader.m_textures.clear();
      if (isFirstModel)
      {
        loader.m_textureAssets = ExportRegisteredTextures(_renderParam.GetTextureAssets());
        isFirstModel = false;
      }
      for (auto &matId : curMesh.scene_mat_ids)
      {
        auto materialObj = _renderParam.v_mat[matId].toMaterialObj();
        loader.m_materials.emplace_back(materialObj);
      }
      vulkan.loadModel(loader);
      const auto instance = vulkan.getInstance(vulkan.getInstanceCount() - 1);
      const size_t firstInstanceId = vulkan.getInstanceCount() - 1;
      const size_t authoredInstanceCount = curMesh.instanceTransforms.size();
      const size_t tlasSlotCount = std::max<size_t>(authoredInstanceCount, 1);
      curMesh.hasInstances = authoredInstanceCount > 0;
      for (size_t i = 1; i < tlasSlotCount; ++i)
      {
        vulkan.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
      }
      curMesh.tlasIds.clear();
      for (size_t i = 0; i < tlasSlotCount; ++i)
      {
        curMesh.tlasIds.push_back(static_cast<int>(i + firstInstanceId));
      }
      _renderParam.MarkMeshTlasDirty(meshId);
    }
    vulkan.addSpheres(_renderParam.v_sphere);
    _renderApp.createRenderResources();
  }
  else if (_resetRenderBuffer)
  {
    _glInteropCache.Clear();
    _renderApp.resize(_width, _height);
    _resetRenderBuffer = false;
  }
}

bool HeadlessRenderBridge::updateMainCamera(const HdRenderPassStateSharedPtr &renderPassState)
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
  _renderApp.getVulkan().setMainCameraClipRange(mainCameraData.clipStart, mainCameraData.clipEnd);

  glm::vec3 camPos = mainCameraData.position;
  glm::vec3 camForward = mainCameraData.forward;
  glm::vec3 camUp = mainCameraData.up;
  glm::vec3 target = camPos + camForward;

  if (glm::length(glm::cross(camForward, camUp)) < 1e-6f)
  {
    camUp = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  const float vfovDeg = std::clamp(mainCameraData.vfov_deg, 1.0f, 179.0f);
  CameraManip.setCamera({camPos, target, camUp, vfovDeg});
  return true;
}

void HeadlessRenderBridge::updateLidarCamera()
{
  const bool lidarEnabledBySetting = GetLidarEnabledSetting(_settings);

  HdRobotLidarData lidarData;
  if (!lidarEnabledBySetting || !_renderParam.GetLidarCamera(&lidarData))
  {
    _renderApp.setLidarEnabled(false);
    return;
  }

  _renderApp.setLidarEnabled(true);

  RayTraceApp::RadarCameraInput lidarCamera{};
  lidarCamera.eye = lidarData.eye;
  lidarCamera.center = lidarData.center;
  lidarCamera.up = lidarData.up;
  lidarCamera.fovDeg = lidarData.fovDeg;
  lidarCamera.lidarParams = {
      lidarData.params.azimuthMinDeg,     lidarData.params.azimuthMaxDeg,  lidarData.params.azimuthStepDeg,
      lidarData.params.verticalMinDeg,    lidarData.params.verticalMaxDeg, lidarData.params.verticalStepDeg,
      lidarData.params.pointRadiusPixels, lidarData.params.maxDistance,
  };
  _renderApp.setRadarCamera(lidarCamera);
}

void HeadlessRenderBridge::updateHeightScanCamera()
{
  HdRobotHeightScanData heightScanData;
  if (!_renderParam.GetHeightScanCamera(&heightScanData))
  {
    _renderApp.setHeightScanEnabled(false);
    return;
  }

  _renderApp.setHeightScanCamera(ToHeightScanCameraInput(heightScanData));

  const HdRobotHeightScanParams &params = heightScanData.params;
  _renderApp.setHeightScanParams({
      params.minX,              params.maxX,
      params.stepX,             params.minZ,
      params.maxZ,              params.stepZ,
      params.rayDirection.x,    params.rayDirection.y,
      params.rayDirection.z,    params.pointRadiusPixels,
      params.maxDistance,       0.0f,
  });
  _renderApp.setHeightScanEnabled(true);
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
  updateBlas();
  updateTlas();
  updateMaterials();
}

void HeadlessRenderBridge::updateBlas()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    if (_renderParam.ConsumeMeshBlasDirty(meshId))
    {
      ConvertVmeshToLoader(_renderParam.v_mesh[meshId], vulkan.m_Loader[meshId]);
      vulkan.updateBlas(meshId);
    }
  }
}

void HeadlessRenderBridge::updateTlas()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  bool updateTlas = false;

  for (size_t meshId = 0; meshId < _renderParam.v_mesh.size(); ++meshId)
  {
    auto &curMesh = _renderParam.v_mesh[meshId];
    if (_renderParam.ConsumeMeshTlasDirty(meshId))
    {
      updateTlas = true;
      const bool tagMatched = isMeshRenderTagMatched(curMesh);
      const bool meshVisible = curMesh.visible && curMesh.valid && tagMatched;
      for (size_t instanceId = 0; instanceId < curMesh.tlasIds.size(); ++instanceId)
      {
        auto tlasId = curMesh.tlasIds[instanceId];
        const bool instanceValid = curMesh.hasInstances && (instanceId < curMesh.instanceTransforms.size());
        const bool visible = meshVisible && instanceValid;
        glm::mat4 transform = glm::transpose(curMesh.transform);
        if (instanceValid)
        {
          transform = glm::transpose(curMesh.transform * curMesh.instanceTransforms[instanceId]);
        }
        vulkan.updateTlas(tlasId, transform, visible);
      }
    }
  }
  if (updateTlas)
  {
    vulkan.updateTlasEnd();
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
