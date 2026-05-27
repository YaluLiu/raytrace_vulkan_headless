#include "heightScanSensor.h"

#include "../hdRobotLidarUsd/heightScanSensor.h"
#include "../hdRobotLidarUsd/tokens.h"
#include "renderParam.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr float kSensorEpsilon = 1.0e-6f;
constexpr float kHeightScanParamEpsilon = 1.0e-4f;

float ClampPositive(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, kHeightScanParamEpsilon);
}

float ClampStep(float value, float fallback)
{
  return std::max(std::fabs(std::isfinite(value) ? value : fallback), kHeightScanParamEpsilon);
}

glm::vec3 NormalizeDirection(glm::vec3 value, glm::vec3 fallback)
{
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
     glm::length(value) < kSensorEpsilon)
  {
    value = fallback;
  }
  if(glm::length(value) < kSensorEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

glm::vec3 ToGlm(const GfVec3f& value)
{
  return glm::vec3(value[0], value[1], value[2]);
}

HdRobotLidarUsdHeightScanSensor::Params ReadRawHeightScanParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
{
  HdRobotLidarUsdHeightScanSensor::Params params;
  if(sceneDelegate == nullptr)
  {
    return params;
  }

  const VtValue paramsValue = sceneDelegate->Get(id, HdRobotLidarUsdTokens->heightScanSensorParams);
  if(paramsValue.IsHolding<HdRobotLidarUsdHeightScanSensor::Params>())
  {
    return paramsValue.UncheckedGet<HdRobotLidarUsdHeightScanSensor::Params>();
  }
  return params;
}

HdRobotHeightScanParams SanitizeHeightScanParams(const HdRobotLidarUsdHeightScanSensor::Params& source)
{
  HdRobotHeightScanParams params;
  params.uStart = source.GetUStart();
  params.uEnd = source.GetUEnd();
  params.uStep = ClampStep(source.GetUStep(), params.uStep);
  params.vStart = source.GetVStart();
  params.vEnd = source.GetVEnd();
  params.vStep = ClampStep(source.GetVStep(), params.vStep);
  params.rayDirection = NormalizeDirection(ToGlm(source.GetRayDirection()), params.rayDirection);
  params.maxRange = ClampPositive(source.GetMaxRange(), params.maxRange);
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

HdRobotHeightScanSensor::HdRobotHeightScanSensor(const SdfPath& id, HdRobotRenderParam& scene)
    : HdSprim(id)
    , _scene(scene)
{
}

HdDirtyBits HdRobotHeightScanSensor::GetInitialDirtyBitsMask() const
{
  return AllDirty;
}

void HdRobotHeightScanSensor::Finalize(HdRenderParam*)
{
  _scene.RemoveHeightScanSensor(GetId());
}

void HdRobotHeightScanSensor::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam*, HdDirtyBits* dirtyBits)
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

  const HdRobotLidarUsdHeightScanSensor::Params rawParams = ReadRawHeightScanParams(sceneDelegate, GetId());
  if(!rawParams.GetEnabled())
  {
    _scene.RemoveHeightScanSensor(GetId());
    *dirtyBits = Clean;
    return;
  }

  HdRobotHeightScanSensorData sensorData;
  sensorData.name = GetId().GetString();
  sensorData.camera = ComputeSensorCameraData(GetId(), sceneDelegate->GetTransform(GetId()));
  sensorData.params = SanitizeHeightScanParams(rawParams);
  _scene.UpsertHeightScanSensor(sensorData);

  *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
