#include "heightScanSensor.h"

#include "../UsdRaySensor/tokens.h"
#include "sceneStore.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

using hdrobot::HeightScanParams;
using hdrobot::HeightScanSensorData;
using hdrobot::HeightScanSensorHandle;
using hdrobot::HeightScanSensorUpdate;
using hdrobot::SceneStore;

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

HeightScanParams SanitizeHeightScanParams(const HeightScanParams& source)
{
  HeightScanParams params;
  params.uStart = source.uStart;
  params.uEnd = source.uEnd;
  params.uStep = ClampStep(source.uStep, params.uStep);
  params.vStart = source.vStart;
  params.vEnd = source.vEnd;
  params.vStep = ClampStep(source.vStep, params.vStep);
  params.gravityDirectionWs = NormalizeDirection(source.gravityDirectionWs, params.gravityDirectionWs);
  params.maxRange = ClampPositive(source.maxRange, params.maxRange);
  return params;
}
} // namespace

HdRobotHeightScanSensor::HdRobotHeightScanSensor(const SdfPath& id,
                                                 SceneStore& sceneStore,
                                                 HeightScanSensorHandle handle)
    : HdSprim(id)
    , _sceneStore(sceneStore)
    , _handle(handle)
{
}

HdDirtyBits HdRobotHeightScanSensor::GetInitialDirtyBitsMask() const
{
  return AllDirty;
}

void HdRobotHeightScanSensor::Finalize(HdRenderParam*)
{
  _sceneStore.DestroyHeightScanSensor(_handle);
}

void HdRobotHeightScanSensor::_SyncParams(HdSceneDelegate* sceneDelegate)
{
  GfVec3f gravityDirectionWs(_params.gravityDirectionWs.x,
                             _params.gravityDirectionWs.y,
                             _params.gravityDirectionWs.z);

  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->enabled, &_enabled, "bool");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->uStart, &_params.uStart, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->uEnd, &_params.uEnd, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->uStep, &_params.uStep, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->vStart, &_params.vStart, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->vEnd, &_params.vEnd, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->vStep, &_params.vStep, "float");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->gravityDirectionWs, &gravityDirectionWs, "GfVec3f");
  ReadCachedParam(sceneDelegate, GetId(), UsdRaySensorTokens->maxRange, &_params.maxRange, "float");

  _params.gravityDirectionWs = ToGlm(gravityDirectionWs);
}

void HdRobotHeightScanSensor::_UpdateRenderParam()
{
  HeightScanSensorData sensorData;
  sensorData.name = GetId().GetString();
  sensorData.camera = HdRobotComputeTransformCameraData(GetId(), _transform);
  sensorData.params = SanitizeHeightScanParams(_params);
  _sceneStore.EnqueueHeightScanSensorUpdate(
      HeightScanSensorUpdate{_handle, sensorData, _enabled});
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
