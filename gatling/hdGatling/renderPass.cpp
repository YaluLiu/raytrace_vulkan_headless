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
#include <iostream>
#include "perf_test/scope_timer.hpp"
#include "nvh/fileoperations.hpp"
#include "nvh/cameramanipulator.hpp"

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
  app_init(hdAovBindings[0]);
  app_updateCamera(*hdcamera);
#if USE_BASE_RENDER
  app_anim_base();
#else
  app_anim_real();
#endif
  _renderApp.render();
#endif

  for(const HdRenderPassAovBinding& binding : hdAovBindings)
  {
    const TfToken&         name         = binding.aovName;
    HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
    if(name == HdAovTokens->color)
    {
#if USE_RAY_TRACE
      renderBuffer->MakeHgiTexture(_renderApp.getVulkan().getOpenGLFrame());
#else
      renderBuffer->change_show_image();
      renderBuffer->ConvertToHgiTexture();
#endif
    }
    else if(name == HdAovTokens->primId)
    {
      renderBuffer->clear(1);
    }
  }
  _frame_idx++;
}

#if USE_RAY_TRACE
void HdGatlingRenderPass::app_init(const HdRenderPassAovBinding& binding)
{
  HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
  _width                              = renderBuffer->GetWidth();
  _height                             = renderBuffer->GetHeight();

  // const GfVec4d &viewport = renderPassState->GetViewport();
  // int width = static_cast<int>(viewport[2]);
  // int height = static_cast<int>(viewport[3]);
  // std::cout << "[HydraInfo]" << width << "," << height << std::endl;
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
      // add_default_material(loader);
      auto& materialObj = _scene.v_mat[cur_mesh.material_id];
      loader.m_materials.emplace_back(materialObj);
      loader.m_textures.clear();
      if(init_texture)
      {
        loader.m_textures.push_back("aMedKitm_albedo.jpg");
        loader.m_textures.push_back("PatrickStar.jpg");
        init_texture = false;
      }
      _renderApp.getVulkan().loadModel(loader, cur_mesh.transform * cur_mesh.instanceTransforms[0]);
      auto instance          = _renderApp.getVulkan().m_instances.back();
      auto first_instance_id = _renderApp.getVulkan().m_instances.size() - 1;
      for(int i = 1; i < cur_mesh.instanceTransforms.size(); i++)
      {
        instance.transform = cur_mesh.transform * cur_mesh.instanceTransforms[i];
        _renderApp.getVulkan().m_instances.push_back(instance);
      }
      for(int i = 0; i < cur_mesh.instanceTransforms.size(); i++)
      {
        cur_mesh.tlasIds.push_back(i + first_instance_id);
      }
    }
#endif
    _renderApp.createBVH();
  }
  else
  {
    _renderApp.resize(_width, _height);
  }
}

void HdGatlingRenderPass::app_updateCamera(const HdCamera& camera)
{
  const GfMatrix4d& transform = camera.GetTransform();

  // 摄像机位置
  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  // 摄像机前向
  GfVec3d forward = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  // 摄像机up
  GfVec3d up = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  // 保证归一化
  forward.Normalize();
  up.Normalize();

  // 构造glm向量
  glm::vec3 camPos(position[0], position[1], position[2]);
  glm::vec3 camForward(forward[0], forward[1], forward[2]);
  glm::vec3 camUp(up[0], up[1], up[2]);
  glm::vec3 target = camPos + camForward;  // 目标点

  // 检查up和forward是否接近共线，避免view矩阵异常
  if(glm::length(glm::cross(camForward, camUp)) < 1e-6)
  {
    // up和forward共线，强制修正up向量
    camUp = glm::vec3(0, 1, 0);  // 或者根据实际场景选择
  }

  // 计算FOV（垂直方向）
  float aperture    = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float vfov        = 2.0f * std::atan(aperture / (2.0f * focalLength));  // 单位：弧度
  float vfov_deg    = glm::degrees(vfov);                                 // 单位：度

  float clipStart = camera.GetClippingRange().GetMin();
  float clipEnd   = camera.GetClippingRange().GetMax();

  // 防止FOV异常
  vfov_deg = std::clamp(vfov_deg, 1.0f, 179.0f);

  // 直接设置矩阵和设置cameraManip是一个效果
  // _renderApp.getVulkan().hydra_viewMatrix = glm::lookAtRH(camPos, target, camUp);
  // float aspectRatio = float(_width)/(float)_height; // 假设宽高比为1.0，需根据实际渲染上下文设置
  // _renderApp.getVulkan().hydra_projMatrix = glm::perspectiveRH_ZO(
  //     vfov,           // 垂直视场角（弧度）
  //     aspectRatio,             // 宽高比
  //     clipStart,      // 近裁剪面
  //     clipEnd         // 远裁剪面
  // );
  // 设置CameraManipulator
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
        auto tlas_id = cur_mesh.tlasIds[ins_id];
        _renderApp.getVulkan().updateTlas(tlas_id, cur_mesh.transform * cur_mesh.instanceTransforms[ins_id], cur_mesh.visible);
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
      new_materials.push_back({static_cast<int>(mesh_id), static_cast<int>(0), materialObj});
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
