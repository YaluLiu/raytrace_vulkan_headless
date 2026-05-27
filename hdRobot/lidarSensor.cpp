#include "lidarSensor.h"

#include "../UsdRaySensor/tokens.h"
#include "renderParam.h"

#include <pxr/base/tf/diagnostic.h>
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

template <typename T>
void ReadCachedParam(HdSceneDelegate* sceneDelegate,
                     const SdfPath& id,
                     const TfToken& key,
                     T* target,
                     const char* expectedType)
{
  if(sceneDelegate == nullptr || target == nullptr)
  {
    return;
  }

  const VtValue value = sceneDelegate->Get(id, key);
  if(value.IsEmpty())
  {
    return;
  }

  if(!value.IsHolding<T>())
  {
    TF_WARN("Ignoring LiDAR sensor parameter %s on <%s>: expected %s, got %s.",
            key.GetText(),
            id.GetText(),
            expectedType,
            value.GetTypeName().c_str());
    return;
  }

  *target = value.UncheckedGet<T>();
}

HdRobotLidarParams SanitizeLidarParams(const HdRobotLidarParams& source)
{
  HdRobotLidarParams params;
  params.azimuthStartDeg = source.azimuthStartDeg;
  params.azimuthEndDeg = source.azimuthEndDeg;
  params.azimuthStepDeg = ClampStep(source.azimuthStepDeg, params.azimuthStepDeg);
  params.verticalStartDeg = source.verticalStartDeg;
  params.verticalEndDeg = source.verticalEndDeg;
  params.verticalStepDeg = ClampStep(source.verticalStepDeg, params.verticalStepDeg);
  params.maxRange = ClampPositive(source.maxRange, params.maxRange);
  params.intensity = ClampNonNegative(source.intensity, params.intensity);
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

void HdRobotLidarSensor::_SyncParams(HdSceneDelegate* sceneDelegate)
{
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->enabled, &_enabled, "bool");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->azimuthStartDeg, &_params.azimuthStartDeg, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->azimuthEndDeg, &_params.azimuthEndDeg, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->azimuthStepDeg, &_params.azimuthStepDeg, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->verticalStartDeg, &_params.verticalStartDeg, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->verticalEndDeg, &_params.verticalEndDeg, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->verticalStepDeg, &_params.verticalStepDeg, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->maxRange, &_params.maxRange, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->intensity, &_params.intensity, "float");
}

void HdRobotLidarSensor::_UpdateRenderParam()
{
  if(!_enabled)
  {
    _scene.RemoveLidarSensor(GetId());
    return;
  }

  HdRobotLidarSensorData sensorData;
  sensorData.name = GetId().GetString();
  sensorData.camera = ComputeSensorCameraData(GetId(), _transform);
  sensorData.params = SanitizeLidarParams(_params);
  _scene.UpsertLidarSensor(sensorData);
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

  if(bits & DirtyTransform)
  {
    _transform = sceneDelegate->GetTransform(GetId());
  }

  if(bits & DirtyParams)
  {
    _SyncParams(sceneDelegate);
  }

  _UpdateRenderParam();

  *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
