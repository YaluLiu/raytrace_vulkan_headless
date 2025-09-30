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

#include <chrono>
#include <thread>
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

#define USE_RAY_TRACE 1
#define USE_BASE_RENDER 0
#define ENABLE_SHARE 1

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
  }
  app_init();
  app_updateCamera(*hdcamera);
  app_updateLight();
#if USE_BASE_RENDER
  app_anim_base();
#else
  app_anim_real();
#endif
  _renderApp.render();
#endif
  _frame_idx++;
}

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
    bool init_texture = true;
    for(auto& cur_mesh : _scene.v_mesh)
    {
      ModelLoader loader;
      ConvertVmeshToLoader(cur_mesh, loader);
      auto& materialObj = _scene.v_mat[cur_mesh.material_id];
      loader.m_materials.emplace_back(materialObj);
      loader.m_textures.clear();
      if(init_texture)
      {
        loader.m_textures.push_back("aMedKitm_albedo.jpg");
        loader.m_textures.push_back("PatrickStar.jpg");
        init_texture = false;
      }
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
    auto& cur_mesh    = _scene.v_mesh[mesh_id];
    auto& mat_id      = cur_mesh.material_id;
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
