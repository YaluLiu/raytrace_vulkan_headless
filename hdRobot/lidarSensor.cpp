#include "lidarSensor.h"

#include "../UsdRaySensor/lidarSensor.h"
#include "../UsdRaySensor/tokens.h"
#include "renderParam.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr float kSensorEpsilon = 1.0e-6f;
constexpr float kLidarParamEpsilon = 1.0e-4f;

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

UsdRaySensorLidarSensor::Params ReadRawLidarParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
{
  UsdRaySensorLidarSensor::Params params;
  if(sceneDelegate == nullptr)
  {
    return params;
  }

  const VtValue paramsValue = sceneDelegate->Get(id, UsdRaySensorTokens->lidarSensorParams);
  if(paramsValue.IsHolding<UsdRaySensorLidarSensor::Params>())
  {
    return paramsValue.UncheckedGet<UsdRaySensorLidarSensor::Params>();
  }
  return params;
}

HdRobotLidarParams SanitizeLidarParams(const UsdRaySensorLidarSensor::Params& source)
{
  HdRobotLidarParams params;
  params.azimuthStartDeg = source.GetAzimuthStartDeg();
  params.azimuthEndDeg = source.GetAzimuthEndDeg();
  params.azimuthStepDeg = ClampStep(source.GetAzimuthStepDeg(), params.azimuthStepDeg);
  params.verticalStartDeg = source.GetVerticalStartDeg();
  params.verticalEndDeg = source.GetVerticalEndDeg();
  params.verticalStepDeg = ClampStep(source.GetVerticalStepDeg(), params.verticalStepDeg);
  params.maxRange = ClampPositive(source.GetMaxRange(), params.maxRange);
  params.intensity = ClampNonNegative(source.GetIntensity(), params.intensity);
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

  if(sceneDelegate == nullptr)
  {
    *dirtyBits = Clean;
    return;
  }

  const UsdRaySensorLidarSensor::Params rawParams = ReadRawLidarParams(sceneDelegate, GetId());
  if(!rawParams.GetEnabled())
  {
    _scene.RemoveLidarSensor(GetId());
    *dirtyBits = Clean;
    return;
  }

  HdRobotLidarSensorData sensorData;
  sensorData.name = GetId().GetString();
  sensorData.camera = ComputeSensorCameraData(GetId(), sceneDelegate->GetTransform(GetId()));
  sensorData.params = SanitizeLidarParams(rawParams);
  _scene.UpsertLidarSensor(sensorData);

  *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
