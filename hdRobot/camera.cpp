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

#include "camera.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
  constexpr float kCameraEpsilon = 1.0e-6f;
  constexpr float kLidarCompareEpsilon = 1.0e-5f;

  LidarParams DefaultLidarParams()
  {
    return {-90.0f, 90.0f, 0.5f, 3.0f, LIDAR_VERTICAL_CHANNEL_CAPACITY, 361, 0.0f, 0.0f, {12.0f, 8.0f, 4.0f, 1.0f, -1.0f, -4.0f, -8.0f, -12.0f}};
  }

  bool NearlyEqual(float a, float b)
  {
    return std::fabs(a - b) <= kLidarCompareEpsilon;
  }

  bool LidarParamsEqual(const LidarParams &a, const LidarParams &b)
  {
    if (!NearlyEqual(a.azimuthMinDeg, b.azimuthMinDeg) || !NearlyEqual(a.azimuthMaxDeg, b.azimuthMaxDeg) || !NearlyEqual(a.azimuthStepDeg, b.azimuthStepDeg) || !NearlyEqual(a.pointRadiusPixels, b.pointRadiusPixels) || a.verticalChannelCount != b.verticalChannelCount || a.horizontalSampleCount != b.horizontalSampleCount)
    {
      return false;
    }

    for (int i = 0; i < LIDAR_VERTICAL_CHANNEL_CAPACITY; ++i)
    {
      if (!NearlyEqual(a.verticalAnglesDeg[i], b.verticalAnglesDeg[i]))
      {
        return false;
      }
    }

    return true;
  }

  bool TryGetFloat(const VtValue &value, float *outValue)
  {
    if (outValue == nullptr || value.IsEmpty())
    {
      return false;
    }

    if (value.IsHolding<float>())
    {
      *outValue = value.UncheckedGet<float>();
      return true;
    }
    if (value.IsHolding<double>())
    {
      *outValue = static_cast<float>(value.UncheckedGet<double>());
      return true;
    }
    if (value.IsHolding<int>())
    {
      *outValue = static_cast<float>(value.UncheckedGet<int>());
      return true;
    }
    if (value.IsHolding<long>())
    {
      *outValue = static_cast<float>(value.UncheckedGet<long>());
      return true;
    }
    if (value.IsHolding<long long>())
    {
      *outValue = static_cast<float>(value.UncheckedGet<long long>());
      return true;
    }
    return false;
  }

  bool TryGetInt(const VtValue &value, int *outValue)
  {
    if (outValue == nullptr || value.IsEmpty())
    {
      return false;
    }

    if (value.IsHolding<int>())
    {
      *outValue = value.UncheckedGet<int>();
      return true;
    }
    if (value.IsHolding<long>())
    {
      *outValue = static_cast<int>(value.UncheckedGet<long>());
      return true;
    }
    if (value.IsHolding<long long>())
    {
      *outValue = static_cast<int>(value.UncheckedGet<long long>());
      return true;
    }
    if (value.IsHolding<float>())
    {
      *outValue = static_cast<int>(std::lround(value.UncheckedGet<float>()));
      return true;
    }
    if (value.IsHolding<double>())
    {
      *outValue = static_cast<int>(std::lround(value.UncheckedGet<double>()));
      return true;
    }
    return false;
  }

  bool TryGetFloatArray(const VtValue &value, std::vector<float> *outValues)
  {
    if (outValues == nullptr || value.IsEmpty())
    {
      return false;
    }

    if (value.IsHolding<VtFloatArray>())
    {
      const VtFloatArray &arr = value.UncheckedGet<VtFloatArray>();
      outValues->assign(arr.begin(), arr.end());
      return true;
    }
    if (value.IsHolding<VtDoubleArray>())
    {
      const VtDoubleArray &arr = value.UncheckedGet<VtDoubleArray>();
      outValues->assign(arr.begin(), arr.end());
      return true;
    }
    if (value.IsHolding<VtIntArray>())
    {
      const VtIntArray &arr = value.UncheckedGet<VtIntArray>();
      outValues->assign(arr.begin(), arr.end());
      return true;
    }
    return false;
  }

  template <size_t N>
  VtValue GetFirstCameraParamValue(HdSceneDelegate *sceneDelegate, const SdfPath &cameraId, const TfToken (&tokenNames)[N])
  {
    if (sceneDelegate == nullptr)
    {
      return {};
    }

    for (const TfToken &tokenName : tokenNames)
    {
      VtValue value = sceneDelegate->GetCameraParamValue(cameraId, tokenName);
      if (!value.IsEmpty())
      {
        return value;
      }
    }
    return {};
  }

  template <size_t N>
  bool TryReadCameraFloat(HdSceneDelegate *sceneDelegate, const SdfPath &cameraId, const TfToken (&tokenNames)[N], float *outValue)
  {
    return TryGetFloat(GetFirstCameraParamValue(sceneDelegate, cameraId, tokenNames), outValue);
  }

  template <size_t N>
  bool TryReadCameraInt(HdSceneDelegate *sceneDelegate, const SdfPath &cameraId, const TfToken (&tokenNames)[N], int *outValue)
  {
    return TryGetInt(GetFirstCameraParamValue(sceneDelegate, cameraId, tokenNames), outValue);
  }

  template <size_t N>
  bool TryReadCameraFloatArray(HdSceneDelegate *sceneDelegate,
                               const SdfPath &cameraId,
                               const TfToken (&tokenNames)[N],
                               std::vector<float> *outValues)
  {
    return TryGetFloatArray(GetFirstCameraParamValue(sceneDelegate, cameraId, tokenNames), outValues);
  }

  LidarParams ReadLidarParams(HdSceneDelegate *sceneDelegate, const SdfPath &cameraId, const LidarParams &defaults)
  {
    static const TfToken kAzimuthMinDegTokens[] = {TfToken("lidar:azimuthMinDeg"), TfToken("azimuthMinDeg"),
                                                   TfToken("lidarAzimuthMinDeg")};
    static const TfToken kAzimuthMaxDegTokens[] = {TfToken("lidar:azimuthMaxDeg"), TfToken("azimuthMaxDeg"),
                                                   TfToken("lidarAzimuthMaxDeg")};
    static const TfToken kAzimuthStepDegTokens[] = {TfToken("lidar:azimuthStepDeg"), TfToken("azimuthStepDeg"),
                                                    TfToken("lidarAzimuthStepDeg")};
    static const TfToken kPointRadiusPixelsTokens[] = {TfToken("lidar:pointRadiusPixels"), TfToken("pointRadiusPixels"),
                                                       TfToken("lidarPointRadiusPixels")};
    static const TfToken kVerticalChannelCountTokens[] = {TfToken("lidar:verticalChannelCount"),
                                                          TfToken("verticalChannelCount"),
                                                          TfToken("lidarVerticalChannelCount")};
    static const TfToken kHorizontalSampleCountTokens[] = {TfToken("lidar:horizontalSampleCount"),
                                                           TfToken("horizontalSampleCount"),
                                                           TfToken("lidarHorizontalSampleCount")};
    static const TfToken kVerticalAnglesDegTokens[] = {TfToken("lidar:verticalAnglesDeg"), TfToken("verticalAnglesDeg"),
                                                       TfToken("lidarVerticalAnglesDeg")};

    LidarParams params = defaults;

    TryReadCameraFloat(sceneDelegate, cameraId, kAzimuthMinDegTokens, &params.azimuthMinDeg);
    TryReadCameraFloat(sceneDelegate, cameraId, kAzimuthMaxDegTokens, &params.azimuthMaxDeg);
    TryReadCameraFloat(sceneDelegate, cameraId, kAzimuthStepDegTokens, &params.azimuthStepDeg);
    TryReadCameraFloat(sceneDelegate, cameraId, kPointRadiusPixelsTokens, &params.pointRadiusPixels);

    int verticalChannelCount = params.verticalChannelCount;
    bool hasVerticalChannelCount =
        TryReadCameraInt(sceneDelegate, cameraId, kVerticalChannelCountTokens, &verticalChannelCount);

    int horizontalSampleCount = params.horizontalSampleCount;
    TryReadCameraInt(sceneDelegate, cameraId, kHorizontalSampleCountTokens, &horizontalSampleCount);

    std::vector<float> verticalAnglesDeg;
    if (TryReadCameraFloatArray(sceneDelegate, cameraId, kVerticalAnglesDegTokens, &verticalAnglesDeg) && !verticalAnglesDeg.empty())
    {
      const int copiedCount = std::min(static_cast<int>(verticalAnglesDeg.size()), LIDAR_VERTICAL_CHANNEL_CAPACITY);
      for (int i = 0; i < copiedCount; ++i)
      {
        params.verticalAnglesDeg[i] = verticalAnglesDeg[i];
      }

      if (!hasVerticalChannelCount)
      {
        verticalChannelCount = copiedCount;
      }
    }

    params.verticalChannelCount = std::clamp(verticalChannelCount, 1, LIDAR_VERTICAL_CHANNEL_CAPACITY);
    params.horizontalSampleCount = std::max(horizontalSampleCount, 1);
    return params;
  }

}

void HdRobotCameraData::PrintLidarParamsForDebug(const std::string &cameraId) const
{
  std::cout << "[LidarParams] camera=" << cameraId << " azimuthStepDeg=" << lidar.azimuthStepDeg
            << " verticalChannelCount=" << lidar.verticalChannelCount << '\n';
}

HdRobotCameraData HdRobotComputeCameraData(const HdCamera &camera)
{
  const GfMatrix4d &transform = camera.GetTransform();

  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  HdRobotCameraData data;
  data.position = glm::vec3(position[0], position[1], position[2]);
  data.forward = glm::vec3(forward[0], forward[1], forward[2]);
  data.up = glm::vec3(up[0], up[1], up[2]);

  if (glm::length(glm::cross(data.forward, data.up)) < kCameraEpsilon)
  {
    data.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  const float aperture = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  const float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float vfov_deg = data.vfov_deg;
  if (camera.GetProjection() == HdCamera::Perspective && focalLength > kCameraEpsilon)
  {
    float vfov = 2.0f * std::atan(aperture / (2.0f * focalLength));
    vfov_deg = glm::degrees(vfov);
  }
  if (!std::isfinite(vfov_deg))
  {
    vfov_deg = 45.0f;
  }
  data.vfov_deg = std::clamp(vfov_deg, 1.0f, 179.0f);

  const GfRange1f clippingRange = camera.GetClippingRange();
  data.clipStart = std::max(clippingRange.GetMin(), 0.001f);
  data.clipEnd = std::max(clippingRange.GetMax(), data.clipStart + 0.001f);
  data.lidar = DefaultLidarParams();

  return data;
}

HdRobotCamera::HdRobotCamera(const SdfPath &id)
    : HdCamera(id)
{
}

HdDirtyBits HdRobotCamera::GetInitialDirtyBitsMask() const
{
  return HdCamera::AllDirty;
}

void HdRobotCamera::Sync(HdSceneDelegate *sceneDelegate, HdRenderParam *renderParam, HdDirtyBits *dirtyBits)
{
  if (!dirtyBits)
  {
    return;
  }

  const HdDirtyBits bits = *dirtyBits;
  HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);

  if ((bits & (DirtyBits::DirtyTransform | DirtyBits::DirtyParams)) == 0)
  {
    return;
  }

  HdRobotCameraData cameraData = HdRobotComputeCameraData(*this);
  cameraData.lidar = ReadLidarParams(sceneDelegate, GetId(), cameraData.lidar);
  cameraData.name = GetId().GetString();
  _cameraData = cameraData;
}

PXR_NAMESPACE_CLOSE_SCOPE
