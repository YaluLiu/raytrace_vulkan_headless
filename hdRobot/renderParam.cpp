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

#include "renderParam.h"

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_OPEN_SCOPE

void HdRobotRenderParam::UpdateLidarCamera(const SdfPath& cameraId, const HdRobotLidarData& lidarData)
{
  std::lock_guard<std::mutex> lock(_lidarMutex);

  if(_hasLidarCamera && _lidarCameraId != cameraId && !_warnedMultipleLidarCameras)
  {
    TF_WARN("Multiple lidar cameras are unsupported (%s -> %s). Latest synced camera wins.", _lidarCameraId.GetText(),
            cameraId.GetText());
    _warnedMultipleLidarCameras = true;
  }

  _lidarCameraId   = cameraId;
  _lidarCameraData = lidarData;
  _hasLidarCamera  = true;
}

void HdRobotRenderParam::ClearLidarCamera(const SdfPath& cameraId)
{
  std::lock_guard<std::mutex> lock(_lidarMutex);
  if(_hasLidarCamera && _lidarCameraId == cameraId)
  {
    _lidarCameraId              = SdfPath();
    _lidarCameraData            = HdRobotLidarData();
    _hasLidarCamera             = false;
    _warnedMultipleLidarCameras = false;
  }
}

bool HdRobotRenderParam::GetLidarCamera(HdRobotLidarData* lidarData) const
{
  std::lock_guard<std::mutex> lock(_lidarMutex);
  if(!_hasLidarCamera || lidarData == nullptr)
  {
    return false;
  }

  *lidarData = _lidarCameraData;
  return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
