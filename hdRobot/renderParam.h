//
// Copyright (C) 2023 Pablo Delgado Krämer
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

#include "camera.h"

#include <pxr/imaging/hd/renderDelegate.h>

#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam final : public HdRenderParam
{
public:
  void UpdateLidarCamera(const SdfPath& cameraId, const HdRobotLidarData& lidarData);
  void ClearLidarCamera(const SdfPath& cameraId);
  bool GetLidarCamera(HdRobotLidarData* lidarData) const;

private:
  mutable std::mutex _lidarMutex;
  SdfPath            _lidarCameraId;
  HdRobotLidarData   _lidarCameraData;
  bool               _hasLidarCamera{ false };
  bool               _warnedMultipleLidarCameras{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
