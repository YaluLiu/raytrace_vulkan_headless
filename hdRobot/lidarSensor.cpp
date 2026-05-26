#include "lidarSensor.h"

#include "renderParam.h"
#include "tokens.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr float kSensorEpsilon = 1.0e-6f;
constexpr float kLidarParamEpsilon = 1.0e-4f;

bool ReadBoolParam(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& token, bool fallback)
{
  if(sceneDelegate == nullptr)
  {
    return fallback;
  }

  const VtValue value = sceneDelegate->Get(id, token);
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

float ReadFloatParam(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& token, float fallback)
{
  if(sceneDelegate == nullptr)
  {
    return fallback;
  }

  const VtValue value = sceneDelegate->Get(id, token);
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

float ClampNonNegative(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, 0.0f);
}

float ClampStep(float value, float fallback)
{
  return std::max(std::fabs(std::isfinite(value) ? value : fallback), kLidarParamEpsilon);
}

HdRobotLidarParams ReadLidarParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
{
  HdRobotLidarParams params;
  params.azimuthStartDeg =
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthStartDeg, params.azimuthStartDeg);
  params.azimuthEndDeg =
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthEndDeg, params.azimuthEndDeg);
  params.azimuthStepDeg = ClampStep(
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthStepDeg, params.azimuthStepDeg),
      params.azimuthStepDeg);
  params.verticalStartDeg =
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalStartDeg, params.verticalStartDeg);
  params.verticalEndDeg =
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalEndDeg, params.verticalEndDeg);
  params.verticalStepDeg = ClampStep(
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalStepDeg, params.verticalStepDeg),
      params.verticalStepDeg);
  params.maxRange = ClampPositive(
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarMaxRange, params.maxRange),
      params.maxRange);
  params.intensity = ClampNonNegative(
      ReadFloatParam(sceneDelegate, id, HdRobotCameraParamTokens->lidarIntensity, params.intensity), params.intensity);
  return params;
}

HdRobotCameraData ComputeSensorCameraData(const SdfPath& id, const GfMatrix4d& transform)
{
  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  HdRobotCameraData data;
  data.name = id.GetString();
  data.position = glm::vec3(position[0], position[1], position[2]);
  data.forward = glm::vec3(forward[0], forward[1], forward[2]);
  data.up = glm::vec3(up[0], up[1], up[2]);
  if(glm::length(glm::cross(data.forward, data.up)) < kSensorEpsilon)
  {
    data.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return data;
}
} // namespace

HdRobotLidarSensor::HdRobotLidarSensor(const SdfPath& id, HdRobotRenderParam& scene)
    : HdSprim(id)
    , _scene(scene)
{
}

HdDirtyBits HdRobotLidarSensor::GetInitialDirtyBitsMask() const
{
  return AllDirty;
}

void HdRobotLidarSensor::Finalize(HdRenderParam*)
{
  _scene.RemoveLidarSensor(GetId());
}

void HdRobotLidarSensor::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam*, HdDirtyBits* dirtyBits)
{
  if(dirtyBits == nullptr)
  {
    return;
  }

  const HdDirtyBits bits = *dirtyBits;
  if((bits & AllDirty) == 0)
  {
    return;
  }

  if(!ReadBoolParam(sceneDelegate, GetId(), HdRobotCameraParamTokens->lidarEnabled, true))
  {
    _scene.RemoveLidarSensor(GetId());
    *dirtyBits = Clean;
    return;
  }

  if(sceneDelegate == nullptr)
  {
    *dirtyBits = Clean;
    return;
  }

  HdRobotLidarSensorData sensorData;
  sensorData.name = GetId().GetString();
  sensorData.camera = ComputeSensorCameraData(GetId(), sceneDelegate->GetTransform(GetId()));
  sensorData.params = ReadLidarParams(sceneDelegate, GetId());
  _scene.UpsertLidarSensor(sensorData);

  *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
