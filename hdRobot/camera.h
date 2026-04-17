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
#include "shaders/host_device.h"

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotCameraData
{
  std::string name;
  glm::vec3   position     = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3   forward      = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3   up           = glm::vec3(0.0f, 1.0f, 0.0f);
  float       vfov_deg     = 45.0f;
  float       clipStart    = 0.1f;
  float       clipEnd      = 1000.0f;
  bool        lidarChanged = false;
  LidarParams lidar        = {-90.0f, 90.0f, 0.5f, 3.0f, LIDAR_VERTICAL_CHANNEL_CAPACITY, 361, 0.0f, 0.0f,
                              {12.0f, 8.0f, 4.0f, 1.0f, -1.0f, -4.0f, -8.0f, -12.0f}};

  void PrintLidarParamsForDebug(const std::string &cameraId) const;
};

HdRobotCameraData HdRobotComputeCameraData(const HdCamera &camera);

class HdRobotCamera final : public HdCamera
{
public:
  explicit HdRobotCamera(const SdfPath &id);

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;
  void Sync(HdSceneDelegate *sceneDelegate, HdRenderParam *renderParam, HdDirtyBits *dirtyBits) override;
  const HdRobotCameraData &GetCameraData() const;

private:
  mutable HdRobotCameraData _cameraData;
};

PXR_NAMESPACE_CLOSE_SCOPE
