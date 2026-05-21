#include "camera.h"

#include "renderParam.h"
#include "tokens.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr float kCameraEpsilon = 1.0e-6f;
constexpr float kLidarParamEpsilon = 1.0e-4f;

bool ReadBoolCameraParam(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& token, bool fallback)
{
  if(sceneDelegate == nullptr)
  {
    return fallback;
  }

  const VtValue value = sceneDelegate->GetCameraParamValue(id, token);
  if(value.IsEmpty())
  {
    return fallback;
  }

  if(value.IsHolding<bool>())
  {
    return value.UncheckedGet<bool>();
  }
  if(value.IsHolding<int>())
  {
    return value.UncheckedGet<int>() != 0;
  }
  if(value.IsHolding<unsigned int>())
  {
    return value.UncheckedGet<unsigned int>() != 0;
  }
  if(value.IsHolding<int64_t>())
  {
    return value.UncheckedGet<int64_t>() != 0;
  }
  if(value.IsHolding<uint64_t>())
  {
    return value.UncheckedGet<uint64_t>() != 0;
  }
  if(value.IsHolding<float>())
  {
    return value.UncheckedGet<float>() != 0.0f;
  }
  if(value.IsHolding<double>())
  {
    return value.UncheckedGet<double>() != 0.0;
  }

  return fallback;
}

float ReadFloatCameraParam(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& token, float fallback)
{
  if(sceneDelegate == nullptr)
  {
    return fallback;
  }

  const VtValue value = sceneDelegate->GetCameraParamValue(id, token);
  float result = fallback;
  if(value.IsEmpty())
  {
    return fallback;
  }

  if(value.IsHolding<float>())
  {
    result = value.UncheckedGet<float>();
  }
  else if(value.IsHolding<double>())
  {
    result = static_cast<float>(value.UncheckedGet<double>());
  }
  else if(value.IsHolding<int>())
  {
    result = static_cast<float>(value.UncheckedGet<int>());
  }
  else if(value.IsHolding<unsigned int>())
  {
    result = static_cast<float>(value.UncheckedGet<unsigned int>());
  }
  else if(value.IsHolding<int64_t>())
  {
    result = static_cast<float>(value.UncheckedGet<int64_t>());
  }
  else if(value.IsHolding<uint64_t>())
  {
    result = static_cast<float>(value.UncheckedGet<uint64_t>());
  }

  return std::isfinite(result) ? result : fallback;
}

float ClampPositive(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, kLidarParamEpsilon);
}

float ClampStep(float value, float fallback)
{
  return std::max(std::fabs(std::isfinite(value) ? value : fallback), kLidarParamEpsilon);
}

HdRobotLidarParams ReadLidarParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
{
  HdRobotLidarParams params;
  params.azimuthMinDeg =
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthMinDeg, params.azimuthMinDeg);
  params.azimuthMaxDeg =
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthMaxDeg, params.azimuthMaxDeg);
  params.azimuthStepDeg = ClampStep(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthStepDeg, params.azimuthStepDeg),
      params.azimuthStepDeg);
  params.verticalMinDeg =
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalMinDeg, params.verticalMinDeg);
  params.verticalMaxDeg =
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalMaxDeg, params.verticalMaxDeg);
  params.verticalStepDeg = ClampStep(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalStepDeg, params.verticalStepDeg),
      params.verticalStepDeg);
  params.pointRadiusPixels = ClampPositive(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarPointRadiusPixels,
                           params.pointRadiusPixels),
      params.pointRadiusPixels);
  params.maxDistance = ClampPositive(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarMaxDistance, params.maxDistance),
      params.maxDistance);
  return params;
}
} // namespace

HdRobotCameraData HdRobotComputeCameraData(const HdCamera& camera)
{
  const GfMatrix4d& transform = camera.GetTransform();

  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward  = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up       = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  HdRobotCameraData data;
  data.position = glm::vec3(position[0], position[1], position[2]);
  data.forward  = glm::vec3(forward[0], forward[1], forward[2]);
  data.up       = glm::vec3(up[0], up[1], up[2]);

  if(glm::length(glm::cross(data.forward, data.up)) < kCameraEpsilon)
  {
    data.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  const float aperture    = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  const float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float       vfovDeg     = data.vfov_deg;
  if(camera.GetProjection() == HdCamera::Perspective && focalLength > kCameraEpsilon)
  {
    const float vfov = 2.0f * std::atan(aperture / (2.0f * focalLength));
    vfovDeg          = glm::degrees(vfov);
  }
  if(!std::isfinite(vfovDeg))
  {
    vfovDeg = 45.0f;
  }
  data.vfov_deg = std::clamp(vfovDeg, 1.0f, 179.0f);

  const GfRange1f clippingRange = camera.GetClippingRange();
  data.clipStart                = std::max(clippingRange.GetMin(), 0.001f);
  data.clipEnd                  = std::max(clippingRange.GetMax(), data.clipStart + 0.001f);

  return data;
}

HdRobotCamera::HdRobotCamera(const SdfPath& id, HdRobotRenderParam& scene)
    : HdCamera(id)
    , _scene(scene)
{
}

HdDirtyBits HdRobotCamera::GetInitialDirtyBitsMask() const
{
  return HdCamera::AllDirty;
}

const HdRobotCameraData& HdRobotCamera::GetCameraData() const
{
  return _cameraData;
}

void HdRobotCamera::Finalize(HdRenderParam*)
{
  _scene.RemoveCamera(GetId());
}

void HdRobotCamera::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  if(!dirtyBits)
  {
    return;
  }

  const HdDirtyBits bits = *dirtyBits;
  HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);

  if((bits & (DirtyBits::DirtyTransform | DirtyBits::DirtyParams)) == 0)
  {
    return;
  }

  HdRobotCameraData cameraData = HdRobotComputeCameraData(*this);
  cameraData.name = GetId().GetString();
  _cameraData = cameraData;
  _scene.UpsertCamera(cameraData);

  const bool isLidar = ReadBoolCameraParam(sceneDelegate, GetId(), HdRobotCameraParamTokens->lidarIsLidar, false);
  if(!isLidar)
  {
    _scene.RemoveLidarSensor(GetId());
    return;
  }

  HdRobotLidarSensorData sensorData;
  sensorData.name = cameraData.name;
  sensorData.camera = cameraData;
  sensorData.params = ReadLidarParams(sceneDelegate, GetId());
  _scene.UpsertLidarSensor(sensorData);
}

PXR_NAMESPACE_CLOSE_SCOPE
