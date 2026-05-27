#include "heightScanSensor.h"

#include "../UsdRaySensor/tokens.h"
#include "renderParam.h"

#include <pxr/base/tf/diagnostic.h>
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
    TF_WARN("Ignoring height scan sensor parameter %s on <%s>: expected %s, got %s.",
            key.GetText(),
            id.GetText(),
            expectedType,
            value.GetTypeName().c_str());
    return;
  }

  *target = value.UncheckedGet<T>();
}

HdRobotHeightScanParams SanitizeHeightScanParams(const HdRobotHeightScanParams& source)
{
  HdRobotHeightScanParams params;
  params.uStart = source.uStart;
  params.uEnd = source.uEnd;
  params.uStep = ClampStep(source.uStep, params.uStep);
  params.vStart = source.vStart;
  params.vEnd = source.vEnd;
  params.vStep = ClampStep(source.vStep, params.vStep);
  params.rayDirection = NormalizeDirection(source.rayDirection, params.rayDirection);
  params.maxRange = ClampPositive(source.maxRange, params.maxRange);
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

void HdRobotHeightScanSensor::_SyncParams(HdSceneDelegate* sceneDelegate)
{
  GfVec3f rayDirection(_params.rayDirection.x, _params.rayDirection.y, _params.rayDirection.z);

  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->enabled, &_enabled, "bool");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->uStart, &_params.uStart, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->uEnd, &_params.uEnd, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->uStep, &_params.uStep, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->vStart, &_params.vStart, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->vEnd, &_params.vEnd, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->vStep, &_params.vStep, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->rayDirection, &rayDirection, "GfVec3f");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->maxRange, &_params.maxRange, "float");

  _params.rayDirection = ToGlm(rayDirection);
}

void HdRobotHeightScanSensor::_UpdateRenderParam()
{
  if(!_enabled)
  {
    _scene.RemoveHeightScanSensor(GetId());
    return;
  }

  HdRobotHeightScanSensorData sensorData;
  sensorData.name = GetId().GetString();
  sensorData.camera = ComputeSensorCameraData(GetId(), _transform);
  sensorData.params = SanitizeHeightScanParams(_params);
  _scene.UpsertHeightScanSensor(sensorData);
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
