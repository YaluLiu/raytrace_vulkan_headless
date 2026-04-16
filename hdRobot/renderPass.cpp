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

bool ensureDirectoryExists(const std::string& path)
{
  try
  {
    if(!std::filesystem::exists(path))
    {
      return std::filesystem::create_directories(path);
    }
    return true;
  }
  catch(const std::filesystem::filesystem_error&)
  {
    return false;
  }
}

PXR_NAMESPACE_OPEN_SCOPE

namespace {
GLuint GetAovSourceGlId(const ::HelloVulkan& app, const TfToken& name)
{
  if(name == HdAovTokens->color)
  {
    return app.m_rtOutputGL.oglId;
  }
  if(name == HdAovTokens->primId)
  {
    return app.m_rtObjectIdGL.oglId;
  }
  if(name == HdAovTokens->instanceId)
  {
    return app.m_rtInstanceIdGL.oglId;
  }
  if(name == HdGatlingAovTokens->dlssRRDiffuseAlbedo)
  {
    return app.m_rtDiffuseAlbedoGL.oglId;
  }
  if(name == HdGatlingAovTokens->dlssRRSpecularAlbedo)
  {
    return app.m_rtSpecularAlbedoGL.oglId;
  }
  if(name == HdGatlingAovTokens->dlssRRNormalRoughness)
  {
    return app.m_rtNormalRoughnessGL.oglId;
  }
  if(name == HdGatlingAovTokens->dlssRRMotionVector)
  {
    return app.m_rtMotionVectorGL.oglId;
  }
  if(name == HdAovTokens->depth || name == HdAovTokens->depthStencil || name == HdGatlingAovTokens->dlssRRLinearDepth)
  {
    return app.m_rtLinearDepthGL.oglId;
  }
  if(name == HdGatlingAovTokens->dlssRRSpecularHitDistance)
  {
    return app.m_rtSpecularHitDistanceGL.oglId;
  }
  if(name == HdGatlingAovTokens->distanceToCamera)
  {
    return app.m_rtDistanceToCameraGL.oglId;
  }
  if(name == HdGatlingAovTokens->lidarPointCloud)
  {
    return app.m_rtLidarPointCloudGL.oglId;
  }
  return 0;
}

void CopyAovToRenderBuffer(const ::HelloVulkan& app, const TfToken& name, HdGatlingRenderBuffer* renderBuffer)
{
  if(renderBuffer == nullptr)
  {
    return;
  }

  const GLuint srcTextureId = GetAovSourceGlId(app, name);
  const GLuint dstTextureId = renderBuffer->get_OpenGL_Texture_id();
  if(srcTextureId == 0 || dstTextureId == 0 || srcTextureId == dstTextureId)
  {
    return;
  }

  const GLsizei width  = static_cast<GLsizei>(renderBuffer->GetWidth());
  const GLsizei height = static_cast<GLsizei>(renderBuffer->GetHeight());
  if(width <= 0 || height <= 0)
  {
    return;
  }

  glCopyImageSubData(srcTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, dstTextureId, GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
}
}  // namespace

HdGatlingRenderPass::HdGatlingRenderPass(HdRenderIndex*             index,
                                         const HdRprimCollection&   collection,
                                         const HdRenderSettingsMap& settings,
                                         HdGatlingScene&            scene,
                                         std::string                resourcePath)
    : HdRenderPass(index, collection)
    , _settings(settings)
    , _isConverged(false)
    , _scene(scene)
    , _resourcePath(std::move(resourcePath))
{}

HdGatlingRenderPass::~HdGatlingRenderPass() {}

bool HdGatlingRenderPass::IsConverged() const
{
  return _isConverged;
}

std::string HdGatlingRenderPass::open_asset(std::string path, int idx)
{
  ArResolver&    resolver     = ArGetResolver();
  ArResolvedPath resolvedPath = resolver.Resolve(path);
  if(!resolvedPath)
  {
    std::cout << "[RenderPass]:" << path << "is not valid" << std::endl;
    return "not valid";
  }


  auto asset = resolver.OpenAsset(resolvedPath);
  if(!asset)
  {
    std::cout << "[RenderPass]:" << resolvedPath.GetPathString() << "failed" << std::endl;
    return "open failed";
  }

  std::string   file_name = "asset_" + std::to_string(idx) + ".jpg";
  std::string   file_path = "media/textures/" + file_name;
  std::ofstream file(file_path, std::ios::binary);
  file.write(static_cast<const char*>(asset->GetBuffer().get()), asset->GetSize());

  if(!file.good())
  {
    std::cout << "[RenderPass] write image file failed" << std::endl;
  }

  file.close();
  return file_name;
}

void HdGatlingRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags)
{
  TF_UNUSED(renderTags);
  if(!app_updateCamera(renderPassState))
  {
    return;
  }

  const auto& hdAovBindings = renderPassState->GetAovBindings();

  HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(hdAovBindings[0].renderBuffer);
  if(_width != renderBuffer->GetWidth() || _height != renderBuffer->GetHeight())
  {
    _width              = renderBuffer->GetWidth();
    _height             = renderBuffer->GetHeight();
    _reset_renderbuffer = true;
  }
  app_init_or_resize();
  app_apply_render_settings();
  app_updateLight();
  app_anim_real();
  _renderApp.render();

  const ::HelloVulkan& app = _renderApp.getVulkan();
  for(const HdRenderPassAovBinding& binding : hdAovBindings)
  {
    auto* aovBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
    CopyAovToRenderBuffer(app, binding.aovName, aovBuffer);
  }
  _frame_idx++;
}

void HdGatlingRenderPass::app_init_or_resize()
{
  if(!_isAppInited)
  {
    _isAppInited = true;
    if(!_resourcePath.empty())
    {
      _renderApp.setPluginSearchRoot(std::filesystem::path(_resourcePath).parent_path().string());
    }
    _renderApp.setup(_width, _height);
    ensureDirectoryExists("media/textures");

    bool is_first_model = true;
    for(auto& cur_mesh : _scene.v_mesh)
    {
      ModelLoader loader;
      ConvertVmeshToLoader(cur_mesh, loader);
      loader.m_textures.clear();
      if(is_first_model)
      {
        for(size_t id = 0; id < _scene.v_texturePath.size(); ++id)
        {
          auto file_name = open_asset(_scene.v_texturePath[id], id);
          loader.m_textures.push_back(file_name);
        }
        is_first_model = false;
      }
      for(auto& mat_id : cur_mesh.scene_mat_ids)
      {
        auto materialObj = _scene.v_mat[mat_id].toMaterialObj();
        loader.m_materials.emplace_back(materialObj);
      }
      // PrintLoader(loader);
      _renderApp.getVulkan().loadModel(loader);
      auto instance          = _renderApp.getVulkan().m_instances.back();
      auto first_instance_id = _renderApp.getVulkan().m_instances.size() - 1;
      for(int i = 1; i < cur_mesh.instanceTransforms.size(); i++)
      {
        _renderApp.getVulkan().m_instances.push_back(instance);
        _renderApp.getVulkan().m_instanceIds.push_back(i);
      }
      for(int i = 0; i < cur_mesh.instanceTransforms.size(); i++)
      {
        cur_mesh.tlasIds.push_back(i + first_instance_id);
      }
    }
    _renderApp.getVulkan().addSpheres(_scene.v_sphere);
    _renderApp.createBVH();
  }
  else if(_reset_renderbuffer)
  {
    _renderApp.resize(_width, _height);
  }
}

void HdGatlingRenderPass::app_apply_render_settings()
{
  HelloVulkan& app = _renderApp.getVulkan();

  if(const auto it = _settings.find(HdGatlingSettingsTokens->spp); it != _settings.end())
  {
    const VtValue& value = it->second;
    if(value.IsHolding<int>())
    {
      app.setSamplesPerFrame(value.UncheckedGet<int>());
    }
    else if(value.IsHolding<unsigned int>())
    {
      app.setSamplesPerFrame(static_cast<int>(value.UncheckedGet<unsigned int>()));
    }
    else if(value.IsHolding<float>())
    {
      app.setSamplesPerFrame(static_cast<int>(value.UncheckedGet<float>()));
    }
    else if(value.IsHolding<double>())
    {
      app.setSamplesPerFrame(static_cast<int>(value.UncheckedGet<double>()));
    }
  }

  if(const auto it = _settings.find(HdGatlingSettingsTokens->dlssRRDenoise); it != _settings.end())
  {
    const VtValue& value = it->second;
    if(value.IsHolding<bool>())
    {
      app.setDlssRREnabled(value.UncheckedGet<bool>());
    }
    else if(value.IsHolding<int>())
    {
      app.setDlssRREnabled(value.UncheckedGet<int>() != 0);
    }
  }
}

void HdGatlingRenderPass::app_updateLight()
{
  _renderApp.getVulkan().clearLights();

  for(auto& cur_light : _scene.v_light)
  {
    if(cur_light.valid)
    {
      _renderApp.getVulkan().addLight(cur_light.toLight());
    }
  }
  if(false)
  {
    Light default_light;
    default_light.type         = 0;  // sphere
    default_light.baseEmission = {15000.0f, 15000.0f, 15000.0f};
    default_light.diffuse      = 1.0;
    default_light.specular     = 1.0;
    default_light.radius       = 1;

    float sphere_radius = 200.0f;  // 大球半径
    int   num_latitude  = 4;       // 纬度方向的数量
    int   num_longitude = 4;       // 经度方向的数量

    for(int i = 0; i < num_latitude; i++)
    {
      float theta = M_PI * (i + 0.5f) / num_latitude;

      for(int j = 0; j < num_longitude; j++)
      {
        float phi              = 2.0f * M_PI * j / num_longitude;
        default_light.position = {sphere_radius * sin(theta) * cos(phi), sphere_radius * sin(theta) * sin(phi),
                                  sphere_radius * cos(theta)};

        _renderApp.getVulkan().addLight(default_light);
      }
    }
  }
}
bool HdGatlingRenderPass::app_updateCamera(const HdRenderPassStateSharedPtr& renderPassState)
{
  const HdCamera* hdcamera = renderPassState ? renderPassState->GetCamera() : nullptr;
  if(!hdcamera)
  {
    return false;
  }

  HydraCamera       cameraData;
  bool              hasCameraData = false;
  const std::string cameraId      = hdcamera->GetId().GetString();
  {
    std::lock_guard guard(_scene.mutex);
    auto            it = _scene.cameraCache.find(cameraId);
    if(it != _scene.cameraCache.end() && it->second.valid)
    {
      cameraData    = it->second;
      hasCameraData = true;
    }
  }

  if(!hasCameraData)
  {
    cameraData = HdGatlingComputeCameraData(*hdcamera);
    std::lock_guard guard(_scene.mutex);
    _scene.cameraCache[cameraId] = cameraData;
  }

  if(!cameraData.valid)
  {
    return false;
  }

  app_updateCameraLidar(hdcamera, cameraData);

  glm::vec3 camPos     = cameraData.position;
  glm::vec3 camForward = cameraData.forward;
  glm::vec3 camUp      = cameraData.up;
  glm::vec3 target     = camPos + camForward;

  if(glm::length(glm::cross(camForward, camUp)) < 1e-6f)
  {
    camUp = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  const float vfov_deg = std::clamp(cameraData.vfov_deg, 1.0f, 179.0f);
  CameraManip.setCamera({camPos, target, camUp, vfov_deg});
  return true;
}

void HdGatlingRenderPass::app_updateCameraLidar(const HdCamera* hdcamera, const HydraCamera& cameraData)
{
  if(hdcamera == nullptr || !cameraData.valid)
  {
    return;
  }

  if(dynamic_cast<const HdGatlingCamera*>(hdcamera) == nullptr)
  {
    return;
  }

  LidarParams lidar = cameraData.lidar;
  lidar.verticalChannelCount   = std::clamp(lidar.verticalChannelCount, 1, LIDAR_VERTICAL_CHANNEL_CAPACITY);
  lidar.horizontalSampleCount = std::max(lidar.horizontalSampleCount, 1);
  _renderApp.getVulkan().m_pcRay.lidar = lidar;
}

void HdGatlingRenderPass::app_anim_real()
{
  app_update_blas();
  app_update_tlas();
  app_update_material();
}

void HdGatlingRenderPass::app_update_blas()
{
  for(size_t mesh_id = 0; mesh_id < _scene.v_mesh.size(); ++mesh_id)
  {
    if(_scene.v_mesh[mesh_id].blas_changed)
    {
      _scene.v_mesh[mesh_id].blas_changed = false;
      ConvertVmeshToLoader(_scene.v_mesh[mesh_id], _renderApp.getVulkan().m_Loader[mesh_id]);
      _renderApp.getVulkan().updateBlas(mesh_id);
    }
  }
}

void HdGatlingRenderPass::app_update_tlas()
{
  bool update_tlas = false;

  for(size_t mesh_id = 0; mesh_id < _scene.v_mesh.size(); ++mesh_id)
  {
    auto& cur_mesh = _scene.v_mesh[mesh_id];
    if(cur_mesh.tlas_changed)
    {
      update_tlas           = true;
      cur_mesh.tlas_changed = false;
      for(int ins_id = 0; ins_id < cur_mesh.instanceTransforms.size(); ins_id++)
      {
        auto      tlas_id   = cur_mesh.tlasIds[ins_id];
        bool      flag_show = cur_mesh.visible & cur_mesh.valid;
        glm::mat4 cur_trans = glm::transpose(cur_mesh.transform * cur_mesh.instanceTransforms[ins_id]);
        _renderApp.getVulkan().updateTlas(tlas_id, cur_trans, flag_show);
      }
    }
  }
  if(update_tlas)
  {
    _renderApp.getVulkan().updateTlasEnd();
  }
}

void HdGatlingRenderPass::app_update_material()
{
  std::vector<MaterialUpdate> new_materials;
  for(size_t mesh_id = 0; mesh_id < _scene.v_mesh.size(); ++mesh_id)
  {
    auto& cur_mesh = _scene.v_mesh[mesh_id];
    for(size_t local_mat_idx = 0; local_mat_idx < cur_mesh.scene_mat_ids.size(); ++local_mat_idx)
    {
      int   global_mat_id = cur_mesh.scene_mat_ids[local_mat_idx];
      auto& materialObj   = _scene.v_mat[global_mat_id];
      if(materialObj.material_changed)
      {
        WaveFrontMaterial new_material;
        new_material.ambient       = materialObj.ambient;
        new_material.diffuse       = materialObj.diffuse;
        new_material.specular      = materialObj.specular;
        new_material.transmittance = materialObj.transmittance;
        new_material.emission      = materialObj.emission;
        new_material.shininess     = materialObj.shininess;
        new_material.ior           = materialObj.ior;
        new_material.dissolve      = materialObj.dissolve;
        new_material.illum         = materialObj.illum;
        new_material.textureId     = materialObj.textureID;
        new_materials.push_back({static_cast<int>(mesh_id), static_cast<int>(local_mat_idx), new_material});
      }
    }
  }
  if(!new_materials.empty())
  {
    _renderApp.getVulkan().updateMaterialsAtRuntime(new_materials);
  }
  for(auto& materialObj : _scene.v_mat)
  {
    materialObj.material_changed = false;
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
