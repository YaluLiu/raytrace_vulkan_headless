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

#include <glm/glm.hpp>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>

#include <memory>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotMesh;
class MaterialNetworkCompiler;
class HdRobotCamera;
class HeadlessRenderBridge;
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

private:
  bool                                _isConverged;
  std::unique_ptr<HeadlessRenderBridge> _bridge;
};

PXR_NAMESPACE_CLOSE_SCOPE
