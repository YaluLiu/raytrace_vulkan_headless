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

#pragma once

#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/camera.h>
#include <string>
#include <vector>
#include <ray_trace_app.hpp>
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotMesh;
class MaterialNetworkCompiler;
class HdRobotCamera;
class HdRobotRenderParam;
struct HdRobotCameraData;

struct GiCameraDesc
{
  float     position[3];
  float     forward[3];
  float     up[3];
  float     vfov;
  float     fStop;
  float     focusDistance;
  float     focalLength;
  float     clipStart;
  float     clipEnd;
  float     exposure;
  glm::mat4 projMatrix;
  glm::mat4 viewMatrix;
};

glm::mat4 computeProjectionMatrix(const GiCameraDesc& cameraDesc, float aspectRatio);

class HdRobotRenderPass final : public HdRenderPass
{
public:
  HdRobotRenderPass(HdRenderIndex*             index,
                    const HdRprimCollection&   collection,
                    const HdRenderSettingsMap& settings,
                    HdRobotRenderParam&        renderParam,
                    std::string                resourcePath);

  ~HdRobotRenderPass() override;

public:
  bool IsConverged() const override;

protected:
  void        _Execute(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags) override;
  std::string open_asset(const std::string& path, int idx);

private:
  const HdRenderSettingsMap& _settings;
  bool                       _isConverged;
  int                        _frame_idx = 0;
  HdRobotRenderParam&        _renderParam;
  std::string                _resourcePath;

  // ----------------------------------------------------------------------------------
  // for headless ray trace app
private:
  bool app_updateMainCamera(const HdRenderPassStateSharedPtr& renderPassState, HdRobotCameraData* cameraData);
  void app_updateLidarCamera();
  void app_updateLight();
  void app_apply_render_settings();
  void app_init_or_resize();

  void app_anim_real();
  void app_update_blas();
  void app_update_tlas();
  void app_update_material();
  bool _UpdateActiveRenderTags(const TfTokenVector& renderTags);
  bool _IsMeshRenderTagMatched(const HydraMesh& mesh) const;

  bool _isAppInited        = false;
  bool _reset_renderbuffer = false;
  TfTokenVector _activeRenderTags;

  RayTraceApp                           _renderApp;
  int _width  = -1;
  int _height = -1;
};

PXR_NAMESPACE_CLOSE_SCOPE

