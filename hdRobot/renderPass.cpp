//
// Copyright (C) 2019-2022 Pablo Delgado Kr盲mer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderPass.h"
#include "renderBuffer.h"
#include "renderParam.h"
#include "renderSettings.h"
#include "renderTextureExport.h"
#include "sceneData.h"
#include "instancer.h"
#include "tokens.h"
#include "camera.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/rprim.h>
#include "nvh/fileoperations.hpp"
#include "nvh/cameramanipulator.hpp"

#include <thread>
#include <filesystem>
#include <sstream>
#include <utility>
#include <algorithm>
#include <mutex>

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
    result.shininess = material.shininess;
    result.ior = material.ior;
    result.dissolve = material.dissolve;
    result.illum = material.illum;
    result.textureId = material.textureID;
    return result;
  }

  TfTokenVector NormalizeRenderTags(TfTokenVector tags)
  {
    std::sort(tags.begin(), tags.end(), [](const TfToken &a, const TfToken &b) {
      return a.GetString() < b.GetString();
    });
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
} // namespace

HdRobotRenderPass::HdRobotRenderPass(HdRenderIndex *index,
                                         const HdRprimCollection &collection,
                                         const HdRenderSettingsMap &settings,
                                         HdRobotRenderParam &renderParam,
                                         std::string resourcePath)
    : HdRenderPass(index, collection)
    , _settings(settings)
    , _isConverged(false)
    , _renderParam(renderParam)
    , _resourcePath(std::move(resourcePath))
{
}

HdRobotRenderPass::~HdRobotRenderPass() {}

bool HdRobotRenderPass::IsConverged() const
{
  return _isConverged;
}

void HdRobotRenderPass::_Execute(const HdRenderPassStateSharedPtr &renderPassState, const TfTokenVector &renderTags)
{
  _isConverged = false;
  if (_UpdateActiveRenderTags(renderTags))
  {
    _renderParam.MarkAllMeshesTlasDirty();
  }

  if (!renderPassState)
  {
    return;
  }

  const auto &hdAovBindings = renderPassState->GetAovBindings();
  HdRobotRenderBuffer *primaryRenderBuffer = GetPrimaryRenderBuffer(hdAovBindings);
  if (primaryRenderBuffer == nullptr)
  {
    return;
  }

  if (_width != static_cast<int>(primaryRenderBuffer->GetWidth()) || _height != static_cast<int>(primaryRenderBuffer->GetHeight()))
  {
    _width = static_cast<int>(primaryRenderBuffer->GetWidth());
    _height = static_cast<int>(primaryRenderBuffer->GetHeight());
    _reset_renderbuffer = true;
  }
  app_apply_render_settings();
  app_init_or_resize();
  HdRobotCameraData mainCameraData;
  if (!app_updateMainCamera(renderPassState, &mainCameraData))
  {
    return;
  }
  app_updateLidarCamera();
  app_updateLight();
  app_anim_real();
  _renderApp.render();

  const ::HelloVulkan &app = _renderApp.getVulkan();
  for (const HdRenderPassAovBinding &binding : hdAovBindings)
  {
    auto *aovBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    CopyAovToRenderBuffer(app, binding.aovName, aovBuffer);
    if (aovBuffer != nullptr)
    {
      aovBuffer->SetConverged(true);
    }
  }
  _isConverged = true;
  _frame_idx++;
}

void HdRobotRenderPass::app_init_or_resize()
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
    _reset_renderbuffer = false;
    EnsureDirectoryExists("media/textures");

    bool is_first_model = true;
    for (size_t mesh_id = 0; mesh_id < _renderParam.v_mesh.size(); ++mesh_id)
    {
      auto& cur_mesh = _renderParam.v_mesh[mesh_id];
      ModelLoader loader;
      ConvertVmeshToLoader(cur_mesh, loader);
      loader.m_textures.clear();
      if (is_first_model)
      {
        loader.m_textures = ExportRegisteredTextures(_renderParam.GetTexturePaths());
        is_first_model = false;
      }
      for (auto &mat_id : cur_mesh.scene_mat_ids)
      {
        auto materialObj = _renderParam.v_mat[mat_id].toMaterialObj();
        loader.m_materials.emplace_back(materialObj);
      }
      vulkan.loadModel(loader);
      const auto   instance          = vulkan.getInstance(vulkan.getInstanceCount() - 1);
      const size_t first_instance_id = vulkan.getInstanceCount() - 1;
      const size_t authoredInstanceCount = cur_mesh.instanceTransforms.size();
      const size_t tlasSlotCount = std::max<size_t>(authoredInstanceCount, 1);
      cur_mesh.hasInstances = authoredInstanceCount > 0;
      for (size_t i = 1; i < tlasSlotCount; ++i)
      {
        vulkan.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
      }
      cur_mesh.tlasIds.clear();
      for (size_t i = 0; i < tlasSlotCount; ++i)
      {
        cur_mesh.tlasIds.push_back(static_cast<int>(i + first_instance_id));
      }
      _renderParam.MarkMeshTlasDirty(mesh_id);
    }
    vulkan.addSpheres(_renderParam.v_sphere);
    _renderApp.createBVH();
  }
  else if (_reset_renderbuffer)
  {
    _renderApp.resize(_width, _height);
    _reset_renderbuffer = false;
  }
}

void HdRobotRenderPass::app_apply_render_settings()
{
  HelloVulkan &app = _renderApp.getVulkan();
  ApplyRenderSettingsToApp(_settings, app);
}

void HdRobotRenderPass::app_updateLight()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  vulkan.clearLights();

  for (auto &cur_light : _renderParam.v_light)
  {
    if (cur_light.valid)
    {
      vulkan.addLight(cur_light.toLight());
    }
  }
}
bool HdRobotRenderPass::app_updateMainCamera(const HdRenderPassStateSharedPtr& renderPassState, HdRobotCameraData* cameraData)
{
  const HdCamera* hdcamera = renderPassState ? renderPassState->GetCamera() : nullptr;
  if (!hdcamera)
  {
    return false;
  }

  const HdRobotCamera* robotCamera = dynamic_cast<const HdRobotCamera*>(hdcamera);
  if (robotCamera == nullptr)
  {
    return false;
  }

  const HdRobotCameraData& mainCameraData = robotCamera->GetCameraData();
  _renderApp.getVulkan().setMainCameraClipRange(mainCameraData.clipStart, mainCameraData.clipEnd);

  glm::vec3 camPos = mainCameraData.position;
  glm::vec3 camForward = mainCameraData.forward;
  glm::vec3 camUp = mainCameraData.up;
  glm::vec3 target = camPos + camForward;

  if (glm::length(glm::cross(camForward, camUp)) < 1e-6f)
  {
    camUp = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  const float vfov_deg = std::clamp(mainCameraData.vfov_deg, 1.0f, 179.0f);
  CameraManip.setCamera({camPos, target, camUp, vfov_deg});
  if(cameraData != nullptr)
  {
    *cameraData = mainCameraData;
  }
  return true;
}

void HdRobotRenderPass::app_updateLidarCamera()
{
  const bool lidarEnabledBySetting = GetLidarEnabledSetting(_settings);

  HdRobotLidarData lidarData;
  if(!lidarEnabledBySetting || !_renderParam.GetLidarCamera(&lidarData))
  {
    _renderApp.setLidarEnabled(false);
    return;
  }

  _renderApp.setLidarEnabled(true);

  RayTraceApp::RadarCameraInput lidarCamera{};
  lidarCamera.eye         = lidarData.eye;
  lidarCamera.center      = lidarData.center;
  lidarCamera.up          = lidarData.up;
  lidarCamera.fovDeg      = lidarData.fovDeg;
  lidarCamera.lidarParams = {
      lidarData.params.azimuthMinDeg,
      lidarData.params.azimuthMaxDeg,
      lidarData.params.azimuthStepDeg,
      lidarData.params.verticalMinDeg,
      lidarData.params.verticalMaxDeg,
      lidarData.params.verticalStepDeg,
      lidarData.params.pointRadiusPixels,
      lidarData.params.maxDistance,
  };
  _renderApp.setRadarCamera(lidarCamera);
}

void HdRobotRenderPass::app_anim_real()
{
  app_update_blas();
  app_update_tlas();
  app_update_material();
}

void HdRobotRenderPass::app_update_blas()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  for (size_t mesh_id = 0; mesh_id < _renderParam.v_mesh.size(); ++mesh_id)
  {
    if (_renderParam.ConsumeMeshBlasDirty(mesh_id))
    {
      ConvertVmeshToLoader(_renderParam.v_mesh[mesh_id], vulkan.m_Loader[mesh_id]);
      vulkan.updateBlas(mesh_id);
    }
  }
}

void HdRobotRenderPass::app_update_tlas()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  bool update_tlas = false;

  for (size_t mesh_id = 0; mesh_id < _renderParam.v_mesh.size(); ++mesh_id)
  {
    auto &cur_mesh = _renderParam.v_mesh[mesh_id];
    if (_renderParam.ConsumeMeshTlasDirty(mesh_id))
    {
      update_tlas = true;
      const bool tagMatched = _IsMeshRenderTagMatched(cur_mesh);
      const bool meshVisible = cur_mesh.visible && cur_mesh.valid && tagMatched;
      for (size_t ins_id = 0; ins_id < cur_mesh.tlasIds.size(); ++ins_id)
      {
        auto tlas_id = cur_mesh.tlasIds[ins_id];
        const bool instanceValid = cur_mesh.hasInstances && (ins_id < cur_mesh.instanceTransforms.size());
        const bool flag_show = meshVisible && instanceValid;
        glm::mat4 cur_trans = glm::transpose(cur_mesh.transform);
        if (instanceValid)
        {
          cur_trans = glm::transpose(cur_mesh.transform * cur_mesh.instanceTransforms[ins_id]);
        }
        vulkan.updateTlas(tlas_id, cur_trans, flag_show);
      }
    }
  }
  if (update_tlas)
  {
    vulkan.updateTlasEnd();
  }
}

bool HdRobotRenderPass::_UpdateActiveRenderTags(const TfTokenVector &renderTags)
{
  TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
  if (RenderTagsEqual(_activeRenderTags, normalizedRenderTags))
  {
    return false;
  }

  _activeRenderTags = std::move(normalizedRenderTags);
  return true;
}

bool HdRobotRenderPass::_IsMeshRenderTagMatched(const HydraMesh &mesh) const
{
  if (_activeRenderTags.empty())
  {
    return true;
  }
  return std::find(_activeRenderTags.begin(), _activeRenderTags.end(), mesh.renderTag) != _activeRenderTags.end();
}

void HdRobotRenderPass::app_update_material()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  std::vector<MaterialUpdate> new_materials;
  for (size_t mesh_id = 0; mesh_id < _renderParam.v_mesh.size(); ++mesh_id)
  {
    auto &cur_mesh = _renderParam.v_mesh[mesh_id];
    for (size_t local_mat_idx = 0; local_mat_idx < cur_mesh.scene_mat_ids.size(); ++local_mat_idx)
    {
      int global_mat_id = cur_mesh.scene_mat_ids[local_mat_idx];
      auto &materialObj = _renderParam.v_mat[global_mat_id];
      if (_renderParam.IsMaterialDirty(static_cast<size_t>(global_mat_id)))
      {
        WaveFrontMaterial new_material = ToWaveFrontMaterial(materialObj);
        new_materials.push_back({static_cast<int>(mesh_id), static_cast<int>(local_mat_idx), new_material});
      }
    }
  }
  if (!new_materials.empty())
  {
    vulkan.updateMaterialsAtRuntime(new_materials);
  }
  _renderParam.ClearAllMaterialDirty();
}

PXR_NAMESPACE_CLOSE_SCOPE
