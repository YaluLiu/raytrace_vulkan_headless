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

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/rprim.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/camera.h>
#include "pxr/imaging/hgiGL/texture.h"
#include <iostream>
#include "perf_test/scope_timer.hpp"
#include "nvh/fileoperations.hpp"
#include "nvh/cameramanipulator.hpp"

// open asset file
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolvedPath.h>

#include <chrono>
#include <thread>
#include <filesystem>

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

HdGatlingRenderPass::HdGatlingRenderPass(HdRenderIndex*             index,
                                         const HdRprimCollection&   collection,
                                         const HdRenderSettingsMap& settings,
                                         HdGatlingScene&            scene)
    : HdRenderPass(index, collection)
    , _settings(settings)
    , _isConverged(false)
    , _scene(scene)
{
  m_startTime = std::chrono::system_clock::now();
}

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

#define USE_RAY_TRACE 1    // render the only color on screen, disable the vk_ray_trace, just color on screen
#define USE_BASE_RENDER 0  // render the default vk_ray_trace image on screen, ignore the usd file
#define ENABLE_SHARE ENABLE_GL_VK_CONVERSION  // disable vulkan->opengl interop
#define TEST_MATERIAL 0

void HdGatlingRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags)
{
  TF_UNUSED(renderTags);
  const HdCamera* hdcamera = renderPassState->GetCamera();
  if(!hdcamera)
  {
    return;
  }
  const auto& hdAovBindings = renderPassState->GetAovBindings();
#if USE_RAY_TRACE

  HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(hdAovBindings[0].renderBuffer);
  if(_width != renderBuffer->GetWidth() || _height != renderBuffer->GetHeight())
  {
    _width              = renderBuffer->GetWidth();
    _height             = renderBuffer->GetHeight();
    _reset_renderbuffer = true;
#if ENABLE_SHARE
    for(const HdRenderPassAovBinding& binding : hdAovBindings)
    {
      const TfToken& name = binding.aovName;
      renderBuffer        = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
      if(name == HdAovTokens->color)
      {
        _renderApp.getVulkan().m_rtOutputGL.oglId = renderBuffer->get_OpenGL_Texture_id();
      }
      else if(name == HdAovTokens->primId)
      {
        _renderApp.getVulkan().m_rtObjectIdGL.oglId = renderBuffer->get_OpenGL_Texture_id();
        renderBuffer->_isIdAov                      = true;
      }
      else if(name == HdAovTokens->instanceId)
      {
        _renderApp.getVulkan().m_rtInstanceIdGL.oglId = renderBuffer->get_OpenGL_Texture_id();
        renderBuffer->_isIdAov                        = true;
      }
    }
#endif
  }
  app_init();
  app_updateCamera(*hdcamera);
  app_updateLight();
#if USE_BASE_RENDER
  app_anim_base();
#else
  app_anim_real();
#endif  //USE_BASE_RENDER
  _renderApp.render();
#if !ENABLE_SHARE
  for(const HdRenderPassAovBinding& binding : hdAovBindings)
  {
    const TfToken& name = binding.aovName;
    renderBuffer        = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
    if(name == HdAovTokens->color)
    {
      renderBuffer->read_color_texture(_renderApp.getVulkan().m_rtOutputGL.oglId);
    }
    else if(name == HdAovTokens->primId)
    {
      renderBuffer->read_object_texture(_renderApp.getVulkan().m_rtObjectIdGL.oglId);
    }
    else if(name == HdAovTokens->instanceId)
    {
      renderBuffer->read_object_texture(_renderApp.getVulkan().m_rtInstanceIdGL.oglId);
    }
    renderBuffer->ConvertToHgiTexture();
  }
#endif  //ENABLE_SHARE
#endif  //USE_RAY_TRACE
  _frame_idx++;
}

#include <utility>
#if USE_RAY_TRACE
void HdGatlingRenderPass::app_init()
{
  if(!_isAppInited)
  {
    _isAppInited = true;
    _renderApp.setup(_width, _height);
#if USE_BASE_RENDER
    _renderApp.loadScene();
#else
    ensureDirectoryExists("media/textures");
    std::map<int, std::string> allTexture;
    for(auto& cur_mesh : _scene.v_mesh)
    {
      for(auto& mat_id : cur_mesh.material_ids)
      {
        if(_scene.v_mat[mat_id].textureID >= 0)
        {
          allTexture[_scene.v_mat[mat_id].textureID] = _scene.v_mat[mat_id].texturePath;
        }
      }
    }
    // std::cout << "-----------------[renderPass]-----------------------------" << std::endl;
    // std::cout << "mesh:" << _scene.v_mesh.size() << std::endl;
    // for(const auto& [id, asset_path] : allTexture)
    // {
    //   std::cout << "allTexture " << id << ": " << asset_path << std::endl;
    // }
    // std::cout << "-----------------[renderPass]-----------------------------" << std::endl;

    bool is_first_model = true;
    for(auto& cur_mesh : _scene.v_mesh)
    {
      ModelLoader loader;
      ConvertVmeshToLoader(cur_mesh, loader);
      loader.m_textures.clear();
      if(is_first_model)
      {
        for(const auto& [id, asset_path] : allTexture)
        {
          auto file_name = open_asset(asset_path, id);
          loader.m_textures.push_back(file_name);
        }
        is_first_model = false;
      }
      for(auto& mat_id : cur_mesh.material_ids)
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
#endif
    _renderApp.getVulkan().addSpheres(_scene.v_sphere);
    _renderApp.createBVH();
  }
  else if(_reset_renderbuffer)
  {
    _renderApp.resize(_width, _height);
  }
}

void printLightCompact(const Light& light)
{
  printf(
      "Light { type=%d, valid=%d, baseEmission=(%.3f,%.3f,%.3f), "
      "diffuse=%.3f, specular=%.3f, dir=(%.3f,%.3f,%.3f), "
      "angleScale=%.3f, pos=(%.3f,%.3f,%.3f), radius=%.3f, "
      "quat=(%.3f,%.3f,%.3f,%.3f) }\n",
      light.type, light.valid, light.baseEmission.x, light.baseEmission.y, light.baseEmission.z, light.diffuseScale,
      light.specularScale, light.direction.x, light.direction.y, light.direction.z, light.angleScale, light.position.x,
      light.position.y, light.position.z, light.radius, light.rotateQuat.x, light.rotateQuat.y, light.rotateQuat.z,
      light.rotateQuat.w);
}

void HdGatlingRenderPass::app_updateLight()
{
  _renderApp.getVulkan().clearLights();

  for(auto& cur_light : _scene.v_light)
  {
    if(cur_light.valid)
    {
      _renderApp.getVulkan().addLight(cur_light);
    }
  }
  //todo:delete this test code
  if(_renderApp.getVulkan().m_lights.size() <= 1)
  {
    Light default_light;
    default_light.type          = 0;  //sphere
    default_light.valid         = 1;
    default_light.baseEmission  = {5000.0f, 5000.0f, 5000.0f};
    default_light.diffuseScale  = 1.0;
    default_light.specularScale = 1.0;
    default_light.radius        = 10;
    default_light.valid         = 1;

    default_light.position = {100, 0, 100};
    _renderApp.getVulkan().addLight(default_light);
    // default_light.position = {0, 100, 100};
    // _renderApp.getVulkan().addLight(default_light);
    // default_light.position = {-100, 0, 100};
    // _renderApp.getVulkan().addLight(default_light);
    // default_light.position = {0, -100, 100};
    // _renderApp.getVulkan().addLight(default_light);
  }
}
void HdGatlingRenderPass::app_updateCamera(const HdCamera& camera)
{
  const GfMatrix4d& transform = camera.GetTransform();

  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward  = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up       = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  glm::vec3 camPos(position[0], position[1], position[2]);
  glm::vec3 camForward(forward[0], forward[1], forward[2]);
  glm::vec3 camUp(up[0], up[1], up[2]);
  glm::vec3 target = camPos + camForward;

  if(glm::length(glm::cross(camForward, camUp)) < 1e-6)
  {
    camUp = glm::vec3(0, 1, 0);
  }

  float aperture    = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float vfov        = 2.0f * std::atan(aperture / (2.0f * focalLength));  // 单位：弧度
  float vfov_deg    = glm::degrees(vfov);                                 // 单位：度

  float clipStart = camera.GetClippingRange().GetMin();
  float clipEnd   = camera.GetClippingRange().GetMax();

  vfov_deg = std::clamp(vfov_deg, 1.0f, 179.0f);
  CameraManip.setCamera({camPos, target, camUp, vfov_deg});
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
    for(auto& mat_id : cur_mesh.material_ids)
    {
      auto& materialObj = _scene.v_mat[mat_id];
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
        new_materials.push_back({static_cast<int>(mesh_id), static_cast<int>(0), new_material});
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
void HdGatlingRenderPass::app_anim_base()
{
  std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
  _renderApp.getVulkan().animationObject(diff.count());
  _renderApp.getVulkan().animationInstances(diff.count());
}
#endif
PXR_NAMESPACE_CLOSE_SCOPE
