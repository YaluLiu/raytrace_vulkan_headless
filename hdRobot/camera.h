//
// Copyright (C) 2026 Pablo Delgado Krämer
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

#include <pxr/imaging/hd/camera.h>
#include <glm/glm.hpp>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotLidarParams
{
  float azimuthMinDeg{ -90.0f };
  float azimuthMaxDeg{ 90.0f };
  float azimuthStepDeg{ 0.5f };
  float verticalMinDeg{ -2.0f };
  float verticalMaxDeg{ -20.0f };
  float verticalStepDeg{ 1.0f };
  float pointRadiusPixels{ 2.0f };
  float maxDistance{ 200.0f };
};

struct HdRobotLidarData
{
  std::string        name;
  glm::vec3          eye{ 0.0f };
  glm::vec3          center{ 0.0f, 0.0f, -1.0f };
  glm::vec3          up{ 0.0f, 1.0f, 0.0f };
  float              fovDeg{ 45.0f };
  HdRobotLidarParams params{};
};

struct HdRobotCameraData
{
  std::string name;
  glm::vec3   position     = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3   forward      = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3   up           = glm::vec3(0.0f, 1.0f, 0.0f);
  float       vfov_deg     = 45.0f;
  float       clipStart    = 0.1f;
  float       clipEnd      = 1000.0f;
  bool        isLidarCamera{ false };
};

HdRobotCameraData HdRobotComputeCameraData(const HdCamera &camera);

class HdRobotCamera final : public HdCamera
{
public:
  explicit HdRobotCamera(const SdfPath &id);

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;
  void Sync(HdSceneDelegate *sceneDelegate, HdRenderParam *renderParam, HdDirtyBits *dirtyBits) override;
  void Finalize(HdRenderParam* renderParam) override;
  const HdRobotCameraData &GetCameraData() const;

private:
  mutable HdRobotCameraData _cameraData;
};

PXR_NAMESPACE_CLOSE_SCOPE
