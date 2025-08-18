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

#pragma once

#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <ray_trace_app.hpp>
#include <renderScene.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdCamera;
class HdGatlingMesh;
class MaterialNetworkCompiler;

struct GiCameraDesc
{
  float position[3];
  float forward[3];
  float up[3];
  float vfov;
  float fStop;
  float focusDistance;
  float focalLength;
  float clipStart;
  float clipEnd;
  float exposure;
  glm::mat4      projMatrix;
  glm::mat4      viewMatrix;
};

glm::mat4 computeProjectionMatrix(const GiCameraDesc& cameraDesc, float aspectRatio);

class HdGatlingRenderPass final : public HdRenderPass
{
public:
  HdGatlingRenderPass(HdRenderIndex* index,
                      const HdRprimCollection& collection,
                      const HdRenderSettingsMap& settings,
                      HdGatlingScene& _scene);

  ~HdGatlingRenderPass() override;

public:
  bool IsConverged() const override;

protected:
  void _Execute(const HdRenderPassStateSharedPtr& renderPassState,
                const TfTokenVector& renderTags) override;

private:
  const HdRenderSettingsMap& _settings;
  bool _isConverged;
  int _frame_idx = 0;
  // 资源场景指针
  HdGatlingScene& _scene;

// ----------------------------------------------------------------------------------
// for headless ray trace app
private:
  void app_updateCamera(const HdCamera& camera) const;
  void app_init(const HdRenderPassAovBinding& binding);
  void app_test_base();

  // 是否初始化了mesh和材质
  bool _isAppInited = false;
  // 渲染框架
  RayTraceApp _renderApp;
  GiCameraDesc _camera;
  std::chrono::system_clock::time_point m_startTime;
};

PXR_NAMESPACE_CLOSE_SCOPE
