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

glm::mat4 computeProjectionMatrix(const GiCameraDesc& cameraDesc, float aspectRatio) {
    // 将垂直视场角转换为弧度
    float fov = glm::radians(cameraDesc.vfov);
    // 获取近裁剪面和远裁剪面
    float near = cameraDesc.clipStart;
    float far = cameraDesc.clipEnd;
    // 计算投影矩阵（右手坐标系，零到一深度）
    glm::mat4 proj = glm::perspectiveRH_ZO(fov, aspectRatio, near, far);
    // 翻转 Y 轴以适应 Vulkan
    proj[1][1] *= -1;
    return proj;
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

void HdGatlingRenderPass::_ConstructGiCamera(const HdCamera& camera, GiCameraDesc& giCamera) const
{
  const GfMatrix4d& transform = camera.GetTransform();

  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  // See https://wiki.panotools.org/Field_of_View
  float aperture = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float vfov = 2.0f * std::atan(aperture / (2.0f * focalLength));

  bool focusOn = true;
#if PXR_VERSION >= 2311
  focusOn = camera.GetFocusOn();
#endif

  giCamera.position[0] = (float) position[0];
  giCamera.position[1] = (float) position[1];
  giCamera.position[2] = (float) position[2];
  giCamera.forward[0] = (float) forward[0];
  giCamera.forward[1] = (float) forward[1];
  giCamera.forward[2] = (float) forward[2];
  giCamera.up[0] = (float) up[0];
  giCamera.up[1] = (float) up[1];
  giCamera.up[2] = (float) up[2];
  giCamera.vfov = vfov;
  giCamera.fStop = float(focusOn) * camera.GetFStop();
  giCamera.focusDistance = camera.GetFocusDistance();
  giCamera.focalLength = focalLength;
  giCamera.clipStart = camera.GetClippingRange().GetMin();
  giCamera.clipEnd = camera.GetClippingRange().GetMax();
  giCamera.exposure = camera.GetExposure();

  glm::vec3 camPos(giCamera.position[0], giCamera.position[1], giCamera.position[2]);
  glm::vec3 camForward(giCamera.forward[0], giCamera.forward[1], giCamera.forward[2]);
  glm::vec3 camUp(giCamera.up[0], giCamera.up[1], giCamera.up[2]);
  glm::vec3 target = camPos + camForward; // 目标点 = 位置 + 朝向
  giCamera.viewMatrix = glm::lookAtRH(camPos, target, camUp);

  // 生成投影矩阵
  float aspectRatio = 1.0f; // 假设宽高比为1.0，需根据实际渲染上下文设置
  giCamera.projMatrix = glm::perspectiveRH_ZO(
      giCamera.vfov,           // 垂直视场角（弧度）
      aspectRatio,             // 宽高比
      giCamera.clipStart,      // 近裁剪面
      giCamera.clipEnd         // 远裁剪面
  );
}
namespace
{
  enum class GiAovId
  {
    Color = 0,
    Normal,
    NEE,
    Barycentrics,
    Texcoords,
    Bounces,
    ClockCycles,
    Opacity,
    Tangents,
    Bitangents,
    ThinWalled,
    ObjectId,
    Depth,
    FaceId,
    InstanceId,
    DoubleSided,
    COUNT
  };

  const static std::unordered_map<TfToken, GiAovId, TfToken::HashFunctor> _aovIdMappings {
    { HdAovTokens->color,                    GiAovId::Color        },
    { HdAovTokens->normal,                   GiAovId::Normal       },
    { HdGatlingAovTokens->debugNee,          GiAovId::NEE          },
    { HdGatlingAovTokens->debugBarycentrics, GiAovId::Barycentrics },
    { HdGatlingAovTokens->debugTexcoords,    GiAovId::Texcoords    },
    { HdGatlingAovTokens->debugBounces,      GiAovId::Bounces      },
    { HdGatlingAovTokens->debugClockCycles,  GiAovId::ClockCycles  },
    { HdGatlingAovTokens->debugOpacity,      GiAovId::Opacity      },
    { HdGatlingAovTokens->debugTangents,     GiAovId::Tangents     },
    { HdGatlingAovTokens->debugBitangents,   GiAovId::Bitangents   },
    { HdGatlingAovTokens->debugThinWalled,   GiAovId::ThinWalled   },
    { HdAovTokens->primId,                   GiAovId::ObjectId     },
    { HdAovTokens->depth,                    GiAovId::Depth        },
    { HdAovTokens->elementId,                GiAovId::FaceId       },
    { HdAovTokens->instanceId,               GiAovId::InstanceId   },
    { HdGatlingAovTokens->debugDoubleSided,  GiAovId::DoubleSided  },
  };

  struct GiAovBinding
  {
    GiAovId         aovId;
    uint8_t         clearValue[16];
  };

  std::vector<GiAovBinding> _PrepareAovBindings(const HdRenderPassAovBindingVector& aovBindings)
  {
    std::vector<GiAovBinding> result;

    for (const HdRenderPassAovBinding& binding : aovBindings)
    {
      HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
      const TfToken& name = binding.aovName;

      auto it = _aovIdMappings.find(name);
      if (it == _aovIdMappings.end())
      {
        TF_RUNTIME_ERROR(TfStringPrintf("Unsupported AOV %s", name.GetText()));
        renderBuffer->SetConverged(true);
        continue;
      }

      auto valueType = HdGetValueTupleType(binding.clearValue).type;
      size_t valueSize = HdDataSizeOfType(valueType);
      const void* valuePtr = HdGetValueData(binding.clearValue);

      GiAovBinding b;
      b.aovId = it->second;
      memcpy(&b.clearValue[0], valuePtr, valueSize);

      result.push_back(b);
    }

    return result;
  }
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

  std::vector<GiAovBinding> aovBindings = _PrepareAovBindings(hdAovBindings);
  if (aovBindings.empty())
  {
    // If this is due to an unsupported AOV, we already logged an error about it.
    return;
  }

  for (const HdRenderPassAovBinding& binding : hdAovBindings)
  {
    const TfToken& name = binding.aovName;
    HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
    auto width = renderBuffer->GetWidth();
    auto height = renderBuffer->GetHeight();
#if USE_RAY_TRACE
    if (!_isAppInited) {
      _isAppInited = true;
      _renderApp.setup(width, height);
      _renderApp.loadScene();
      _renderApp.createBVH();
    } else {
      _renderApp.resize(width,height);
    }
#endif
  }

  std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
  _renderApp.getVulkan().animationObject(diff.count());
  _renderApp.getVulkan().animationInstances(diff.count());
  _renderApp.render();
  for (const HdRenderPassAovBinding& binding : hdAovBindings)
  {
    const TfToken& name = binding.aovName;
    HdGatlingRenderBuffer* renderBuffer = static_cast<HdGatlingRenderBuffer*>(binding.renderBuffer);
    auto width = renderBuffer->GetWidth();
    auto height = renderBuffer->GetHeight();
    if(name == HdAovTokens->color) {
#if USE_RAY_TRACE
      renderBuffer->MakeHgiTexture(_renderApp.getOpenGLFrame());
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

void HdGatlingRenderPass::init_render_app()
{
//   if (_isAppInited){
//     return;
//   }
//   _isAppInited = true;
// #if USE_RAY_TRACE
// #endif
}

PXR_NAMESPACE_CLOSE_SCOPE
