#include "camera.h"

#include "renderParam.h"
#include "tokens.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec3f.h>
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

glm::vec3 ReadVec3CameraParam(HdSceneDelegate* sceneDelegate,
                              const SdfPath& id,
                              const TfToken& token,
                              glm::vec3 fallback)
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

  glm::vec3 result = fallback;
  if(value.IsHolding<GfVec3f>())
  {
    const GfVec3f vec = value.UncheckedGet<GfVec3f>();
    result = glm::vec3(vec[0], vec[1], vec[2]);
  }
  else if(value.IsHolding<GfVec3d>())
  {
    const GfVec3d vec = value.UncheckedGet<GfVec3d>();
    result = glm::vec3(static_cast<float>(vec[0]), static_cast<float>(vec[1]), static_cast<float>(vec[2]));
  }

  return std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z) ? result : fallback;
}

float ClampPositive(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, kLidarParamEpsilon);
}

float ClampStep(float value, float fallback)
{
  return std::max(std::fabs(std::isfinite(value) ? value : fallback), kLidarParamEpsilon);
}

glm::vec3 NormalizeDirection(glm::vec3 value, glm::vec3 fallback)
{
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)
     || glm::length(value) < kCameraEpsilon)
  {
    value = fallback;
  }
  if(glm::length(value) < kCameraEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

HdRobotHeightScanParams ReadHeightScanParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
{
  HdRobotHeightScanParams params;
  params.minX = ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMinX, params.minX);
  params.maxX = ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMaxX, params.maxX);
  params.stepX = ClampStep(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanStepX, params.stepX),
      params.stepX);
  params.minZ = ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMinZ, params.minZ);
  params.maxZ = ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMaxZ, params.maxZ);
  params.stepZ = ClampStep(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanStepZ, params.stepZ),
      params.stepZ);
  params.rayDirection = NormalizeDirection(
      ReadVec3CameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanRayDirection, params.rayDirection),
      params.rayDirection);
  params.maxDistance = ClampPositive(
      ReadFloatCameraParam(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMaxDistance, params.maxDistance),
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

  if(ReadBoolCameraParam(sceneDelegate, GetId(), HdRobotCameraParamTokens->heightScanIsHeightScan, false))
  {
    HdRobotHeightScanSensorData sensorData;
    sensorData.name = cameraData.name;
    sensorData.camera = cameraData;
    sensorData.params = ReadHeightScanParams(sceneDelegate, GetId());
    _scene.UpsertHeightScanSensor(sensorData);
  }
  else
  {
    _scene.RemoveHeightScanSensor(GetId());
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
