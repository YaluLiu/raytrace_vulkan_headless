//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
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
#include "instancer.h"
#include "tokens.h"
#include "camera.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/rprim.h>
#include "pxr/imaging/hgiGL/texture.h"
#include <iostream>
#include "nvh/fileoperations.hpp"
#include "nvh/cameramanipulator.hpp"
#include <nvgl/extensions_gl.hpp>

// open asset file
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolvedPath.h>

#include <thread>
#include <filesystem>
#include <sstream>
#include <utility>
#include <algorithm>
#include <mutex>
#include <fstream>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
  bool EnsureDirectoryExists(const std::string &path)
  {
    try
    {
      if (!std::filesystem::exists(path))
      {
        return std::filesystem::create_directories(path);
      }
      return true;
    }
    catch (const std::filesystem::filesystem_error &)
    {
      return false;
    }
  }

  HdRobotRenderBuffer *GetPrimaryRenderBuffer(const HdRenderPassAovBindingVector &bindings)
  {
    for (const HdRenderPassAovBinding &binding : bindings)
    {
      if (binding.renderBuffer != nullptr)
      {
        return static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
      }
    }
    return nullptr;
  }

  GLuint GetAovSourceGlId(const ::HelloVulkan &app, const TfToken &name)
  {
    if (name == HdAovTokens->color)
    {
      return app.m_rtOutputGL.oglId;
    }
    if (name == HdAovTokens->primId)
    {
      return app.m_rtObjectIdGL.oglId;
    }
    if (name == HdAovTokens->instanceId)
    {
      return app.m_rtInstanceIdGL.oglId;
    }
    if (name == HdRobotAovTokens->dlssRRDiffuseAlbedo)
    {
      return app.m_rtDiffuseAlbedoGL.oglId;
    }
    if (name == HdRobotAovTokens->dlssRRSpecularAlbedo)
    {
      return app.m_rtSpecularAlbedoGL.oglId;
    }
    if (name == HdRobotAovTokens->dlssRRNormalRoughness)
    {
      return app.m_rtNormalRoughnessGL.oglId;
    }
    if (name == HdRobotAovTokens->dlssRRMotionVector)
    {
      return app.m_rtMotionVectorGL.oglId;
    }
    if (name == HdAovTokens->depth || name == HdAovTokens->depthStencil || name == HdRobotAovTokens->dlssRRLinearDepth)
    {
      return app.m_rtLinearDepthGL.oglId;
    }
    if (name == HdRobotAovTokens->dlssRRSpecularHitDistance)
    {
      return app.m_rtSpecularHitDistanceGL.oglId;
    }
    if (name == HdRobotAovTokens->distanceToCamera)
    {
      return app.m_rtDistanceToCameraGL.oglId;
    }
    if (name == HdRobotAovTokens->lidarPointCloud)
    {
      return app.m_rtLidarPointCloudGL.oglId;
    }
    return 0;
  }

  void CopyAovToRenderBuffer(const ::HelloVulkan &app, const TfToken &name, HdRobotRenderBuffer *renderBuffer)
  {
    if (renderBuffer == nullptr)
    {
      return;
    }

    const GLuint srcTextureId = GetAovSourceGlId(app, name);
    const GLuint dstTextureId = renderBuffer->GetOpenGlTextureId();
    if (srcTextureId == 0 || dstTextureId == 0 || srcTextureId == dstTextureId)
    {
      return;
    }

    const GLsizei width = static_cast<GLsizei>(renderBuffer->GetWidth());
    const GLsizei height = static_cast<GLsizei>(renderBuffer->GetHeight());
    if (width <= 0 || height <= 0)
    {
      return;
    }

    glCopyImageSubData(srcTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, dstTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
  }

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
                                         HdRobotScene &scene,
                                         std::string resourcePath)
    : HdRenderPass(index, collection), _settings(settings), _isConverged(false), _scene(scene), _resourcePath(std::move(resourcePath))
{
}

HdRobotRenderPass::~HdRobotRenderPass() {}

bool HdRobotRenderPass::IsConverged() const
{
  return _isConverged;
}

std::string HdRobotRenderPass::open_asset(const std::string &path, int idx)
{
  ArResolver &resolver = ArGetResolver();
  ArResolvedPath resolvedPath = resolver.Resolve(path);
  if (!resolvedPath)
  {
    std::cout << "[RenderPass]:" << path << "is not valid" << std::endl;
    return "not valid";
  }

  auto asset = resolver.OpenAsset(resolvedPath);
  if (!asset)
  {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "failed" << std::endl;
    return "open failed";
  }

  std::string file_name = "asset_" + std::to_string(idx) + ".jpg";
  std::string file_path = "media/textures/" + file_name;
  std::ofstream file(file_path, std::ios::binary);
  file.write(static_cast<const char *>(asset->GetBuffer().get()), asset->GetSize());

  if (!file.good())
  {
    std::cout << "[RenderPass] write image file failed" << std::endl;
  }

  file.close();
  return file_name;
}

void HdRobotRenderPass::_Execute(const HdRenderPassStateSharedPtr &renderPassState, const TfTokenVector &renderTags)
{
  if (_UpdateActiveRenderTags(renderTags))
  {
    for (auto &mesh : _scene.v_mesh)
    {
      mesh.tlas_changed = true;
    }
  }

  if (!renderPassState || !app_updateCamera(renderPassState))
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
  app_init_or_resize();
  app_apply_render_settings();
  app_updateLight();
  app_anim_real();
  _renderApp.render();

  const ::HelloVulkan &app = _renderApp.getVulkan();
  for (const HdRenderPassAovBinding &binding : hdAovBindings)
  {
    auto *aovBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    CopyAovToRenderBuffer(app, binding.aovName, aovBuffer);
  }
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
    for (auto &cur_mesh : _scene.v_mesh)
    {
      ModelLoader loader;
      ConvertVmeshToLoader(cur_mesh, loader);
      loader.m_textures.clear();
      if (is_first_model)
      {
        const auto& texturePaths = _scene.GetTexturePaths();
        for (size_t id = 0; id < texturePaths.size(); ++id)
        {
          auto file_name = open_asset(texturePaths[id], id);
          loader.m_textures.push_back(file_name);
        }
        is_first_model = false;
      }
      for (auto &mat_id : cur_mesh.scene_mat_ids)
      {
        auto materialObj = _scene.v_mat[mat_id].toMaterialObj();
        loader.m_materials.emplace_back(materialObj);
      }
      // PrintLoader(loader);
      vulkan.loadModel(loader);
      auto instance = vulkan.m_instances.back();
      const size_t first_instance_id = vulkan.m_instances.size() - 1;
      const size_t authoredInstanceCount = cur_mesh.instanceTransforms.size();
      const size_t tlasSlotCount = std::max<size_t>(authoredInstanceCount, 1);
      cur_mesh.hasInstances = authoredInstanceCount > 0;
      for (size_t i = 1; i < tlasSlotCount; ++i)
      {
        vulkan.m_instances.push_back(instance);
        vulkan.m_instanceIds.push_back(static_cast<int>(i));
      }
      cur_mesh.tlasIds.clear();
      for (size_t i = 0; i < tlasSlotCount; ++i)
      {
        cur_mesh.tlasIds.push_back(static_cast<int>(i + first_instance_id));
      }
      cur_mesh.tlas_changed = true;
    }
    vulkan.addSpheres(_scene.v_sphere);
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

  if (const auto it = _settings.find(HdRobotSettingsTokens->spp); it != _settings.end())
  {
    const VtValue &value = it->second;
    if (value.IsHolding<int>())
    {
      app.setSamplesPerFrame(value.UncheckedGet<int>());
    }
    else if (value.IsHolding<unsigned int>())
    {
      app.setSamplesPerFrame(static_cast<int>(value.UncheckedGet<unsigned int>()));
    }
    else if (value.IsHolding<float>())
    {
      app.setSamplesPerFrame(static_cast<int>(value.UncheckedGet<float>()));
    }
    else if (value.IsHolding<double>())
    {
      app.setSamplesPerFrame(static_cast<int>(value.UncheckedGet<double>()));
    }
  }

  if (const auto it = _settings.find(HdRobotSettingsTokens->dlssRRDenoise); it != _settings.end())
  {
    const VtValue &value = it->second;
    if (value.IsHolding<bool>())
    {
      app.setDlssRREnabled(value.UncheckedGet<bool>());
    }
    else if (value.IsHolding<int>())
    {
      app.setDlssRREnabled(value.UncheckedGet<int>() != 0);
    }
  }
}

void HdRobotRenderPass::app_updateLight()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  vulkan.clearLights();

  for (auto &cur_light : _scene.v_light)
  {
    if (cur_light.valid)
    {
      vulkan.addLight(cur_light.toLight());
    }
  }
}
bool HdRobotRenderPass::app_updateCamera(const HdRenderPassStateSharedPtr &renderPassState)
{
  const HdCamera *hdcamera = renderPassState ? renderPassState->GetCamera() : nullptr;
  if (!hdcamera)
  {
    return false;
  }

  const HdRobotCamera *robotCamera = dynamic_cast<const HdRobotCamera *>(hdcamera);
  if (robotCamera == nullptr)
  {
    return false;
  }

  const HdRobotCameraData &cameraData = robotCamera->GetCameraData();
  app_updateCameraLidar(cameraData);

  glm::vec3 camPos = cameraData.position;
  glm::vec3 camForward = cameraData.forward;
  glm::vec3 camUp = cameraData.up;
  glm::vec3 target = camPos + camForward;

  if (glm::length(glm::cross(camForward, camUp)) < 1e-6f)
  {
    camUp = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  const float vfov_deg = std::clamp(cameraData.vfov_deg, 1.0f, 179.0f);
  CameraManip.setCamera({camPos, target, camUp, vfov_deg});
  return true;
}

void HdRobotRenderPass::app_updateCameraLidar(const HdRobotCameraData &cameraData)
{
  LidarParams lidar = cameraData.lidar;
  lidar.verticalChannelCount = std::clamp(lidar.verticalChannelCount, 1, LIDAR_VERTICAL_CHANNEL_CAPACITY);
  lidar.horizontalSampleCount = std::max(lidar.horizontalSampleCount, 1);
  _renderApp.getVulkan().m_pcRay.lidar = lidar;
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
  for (size_t mesh_id = 0; mesh_id < _scene.v_mesh.size(); ++mesh_id)
  {
    if (_scene.v_mesh[mesh_id].blas_changed)
    {
      _scene.v_mesh[mesh_id].blas_changed = false;
      ConvertVmeshToLoader(_scene.v_mesh[mesh_id], vulkan.m_Loader[mesh_id]);
      vulkan.updateBlas(mesh_id);
    }
  }
}

void HdRobotRenderPass::app_update_tlas()
{
  HelloVulkan &vulkan = _renderApp.getVulkan();
  bool update_tlas = false;

  for (size_t mesh_id = 0; mesh_id < _scene.v_mesh.size(); ++mesh_id)
  {
    auto &cur_mesh = _scene.v_mesh[mesh_id];
    if (cur_mesh.tlas_changed)
    {
      update_tlas = true;
      cur_mesh.tlas_changed = false;
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
  for (size_t mesh_id = 0; mesh_id < _scene.v_mesh.size(); ++mesh_id)
  {
    auto &cur_mesh = _scene.v_mesh[mesh_id];
    for (size_t local_mat_idx = 0; local_mat_idx < cur_mesh.scene_mat_ids.size(); ++local_mat_idx)
    {
      int global_mat_id = cur_mesh.scene_mat_ids[local_mat_idx];
      auto &materialObj = _scene.v_mat[global_mat_id];
      if (materialObj.material_changed)
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
  for (auto &materialObj : _scene.v_mat)
  {
    materialObj.material_changed = false;
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
