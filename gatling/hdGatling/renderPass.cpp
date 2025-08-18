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

HdGatlingRenderPass::HdGatlingRenderPass(HdRenderIndex* index,
                                         const HdRprimCollection& collection,
                                         const HdRenderSettingsMap& settings,
                                        HdGatlingScene& scene)
  : HdRenderPass(index, collection)
  , _settings(settings)
  , _isConverged(false)
  , _scene(scene)
{
  m_startTime = std::chrono::system_clock::now();
}

HdGatlingRenderPass::~HdGatlingRenderPass()
{
}

bool HdGatlingRenderPass::IsConverged() const
{
  return _isConverged;
}

void printMatrix(const glm::mat4& matrix, const std::string& name) {
    std::cout << name << ":\n";
    for (int i = 0; i < 4; ++i) {
        std::cout << std::fixed << std::setprecision(4); // Set precision for readability
        std::cout << "| ";
        for (int j = 0; j < 4; ++j) {
            std::cout << std::setw(8) << matrix[i][j] << " "; // Access column-major elements
        }
        std::cout << "|\n";
    }
    std::cout << "\n";
}

void PrintVec3(const std::string& name, const glm::vec3& v) {
    std::cout << name << ": (" << v.x << ", " << v.y << ", " << v.z << ")" << std::endl;
}

#define USE_RAY_TRACE 1
void HdGatlingRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState,
                                  const TfTokenVector& renderTags)
{
  TF_UNUSED(renderTags);
  const HdCamera* hdcamera = renderPassState->GetCamera();
  if (!hdcamera)
  {
    return;
  }
  const auto& hdAovBindings = renderPassState->GetAovBindings();
#if USE_RAY_TRACE
  app_init(hdAovBindings[0]);
  app_updateCamera(*hdcamera);
  app_test_base();
#endif

  for (const HdRenderPassAovBinding& binding : hdAovBindings)
  {
    const TfToken& name = binding.aovName;
    HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
    if(name == HdAovTokens->color) {
#if USE_RAY_TRACE
      renderBuffer->MakeHgiTexture(_renderApp.getVulkan().getOpenGLFrame());
#else
      renderBuffer->change_show_image();
      renderBuffer->ConvertToHgiTexture();
#endif
    } else if(name == HdAovTokens->primId) {
      renderBuffer->clear(1);
    }
  }
  _frame_idx++;
}

#if USE_RAY_TRACE
void HdGatlingRenderPass::app_init(const HdRenderPassAovBinding& binding)
{
  HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
  auto width = renderBuffer->GetWidth();
  auto height = renderBuffer->GetHeight();
  // const GfVec4d &viewport = renderPassState->GetViewport();
  // int width = static_cast<int>(viewport[2]);
  // int height = static_cast<int>(viewport[3]);
  // std::cout << "[HydraInfo]" << width << "," << height << std::endl;
  if (!_isAppInited) {
    _isAppInited = true;
    _renderApp.setup(width, height);
    _renderApp.loadScene();
    _renderApp.createBVH();
  } else {
    _renderApp.resize(width,height);
  }
}

void HdGatlingRenderPass::app_updateCamera(const HdCamera& camera) const
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
    glm::vec3 camCenter = camPos + camForward; // 目标点

    // 检查up和forward是否接近共线，避免view矩阵异常
    if (glm::length(glm::cross(camForward, camUp)) < 1e-6) {
        // up和forward共线，强制修正up向量
        camUp = glm::vec3(0, 1, 0); // 或者根据实际场景选择
    }

    // 计算FOV（垂直方向）
    float aperture = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
    float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
    float vfov = 2.0f * std::atan(aperture / (2.0f * focalLength)); // 单位：弧度
    float vfov_deg = glm::degrees(vfov); // 单位：度

    // 防止FOV异常
    vfov_deg = std::clamp(vfov_deg, 1.0f, 179.0f);

    // 设置CameraManipulator
    CameraManip.setCamera({camPos, camCenter, camUp, vfov_deg});
}

void HdGatlingRenderPass::app_test_base()
{
  std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
  _renderApp.getVulkan().animationObject(diff.count());
  _renderApp.getVulkan().animationInstances(diff.count());
  _renderApp.render();
}
#endif
PXR_NAMESPACE_CLOSE_SCOPE
