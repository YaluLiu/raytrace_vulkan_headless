#include "camera.h"
#include "renderParam.h"
#include "tokens.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
  constexpr float kCameraEpsilon = 1.0e-6f;

  bool ReadOptionalBool(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& key, bool defaultValue)
  {
    if(sceneDelegate == nullptr)
    {
      return defaultValue;
    }

    const VtValue value = sceneDelegate->Get(id, key);
    if(value.IsEmpty())
    {
      return defaultValue;
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
      return value.UncheckedGet<unsigned int>() != 0u;
    }

    TF_WARN("%s:%s does not hold a supported bool-like type; using default", id.GetText(), key.GetText());
    return defaultValue;
  }

  float ReadOptionalFloat(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& key, float defaultValue)
  {
    if(sceneDelegate == nullptr)
    {
      return defaultValue;
    }

    const VtValue value = sceneDelegate->Get(id, key);
    if(value.IsEmpty())
    {
      return defaultValue;
    }
    if(value.IsHolding<float>())
    {
      return value.UncheckedGet<float>();
    }
    if(value.IsHolding<double>())
    {
      return static_cast<float>(value.UncheckedGet<double>());
    }
    if(value.IsHolding<int>())
    {
      return static_cast<float>(value.UncheckedGet<int>());
    }
    if(value.IsHolding<unsigned int>())
    {
      return static_cast<float>(value.UncheckedGet<unsigned int>());
    }

    TF_WARN("%s:%s does not hold a supported numeric type; using default", id.GetText(), key.GetText());
    return defaultValue;
  }

  glm::vec3 ReadOptionalVec3(HdSceneDelegate* sceneDelegate, const SdfPath& id, const TfToken& key,
                             const glm::vec3& defaultValue)
  {
    if(sceneDelegate == nullptr)
    {
      return defaultValue;
    }

    const VtValue value = sceneDelegate->Get(id, key);
    if(value.IsEmpty())
    {
      return defaultValue;
    }
    if(value.IsHolding<GfVec3f>())
    {
      const GfVec3f& vec = value.UncheckedGet<GfVec3f>();
      return glm::vec3(vec[0], vec[1], vec[2]);
    }
    if(value.IsHolding<GfVec3d>())
    {
      const GfVec3d& vec = value.UncheckedGet<GfVec3d>();
      return glm::vec3(static_cast<float>(vec[0]), static_cast<float>(vec[1]), static_cast<float>(vec[2]));
    }

    TF_WARN("%s:%s does not hold a supported vec3 type; using default", id.GetText(), key.GetText());
    return defaultValue;
  }

  float SanitizeFiniteFloat(float value, float defaultValue)
  {
    return std::isfinite(value) ? value : defaultValue;
  }

  float SanitizePositiveFloat(float value, float defaultValue, float minValue)
  {
    value = SanitizeFiniteFloat(value, defaultValue);
    return std::max(value, minValue);
  }

  glm::vec3 SanitizeFiniteVec3(const glm::vec3& value, const glm::vec3& defaultValue)
  {
    if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
    {
      return defaultValue;
    }
    return value;
  }

  glm::vec3 SanitizeDirection(const glm::vec3& value, const glm::vec3& defaultValue)
  {
    const glm::vec3 finiteValue = SanitizeFiniteVec3(value, defaultValue);
    if(glm::dot(finiteValue, finiteValue) < kCameraEpsilon * kCameraEpsilon)
    {
      return defaultValue;
    }
    return glm::normalize(finiteValue);
  }

  HdRobotLidarParams ReadLidarParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
  {
    HdRobotLidarParams params;
    params.azimuthMinDeg =
        SanitizeFiniteFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthMinDeg, params.azimuthMinDeg),
                            params.azimuthMinDeg);
    params.azimuthMaxDeg =
        SanitizeFiniteFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthMaxDeg, params.azimuthMaxDeg),
                            params.azimuthMaxDeg);
    params.azimuthStepDeg =
        SanitizePositiveFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarAzimuthStepDeg, params.azimuthStepDeg),
                              params.azimuthStepDeg, 1.0e-4f);
    params.verticalMinDeg =
        SanitizeFiniteFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalMinDeg, params.verticalMinDeg),
                            params.verticalMinDeg);
    params.verticalMaxDeg =
        SanitizeFiniteFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalMaxDeg, params.verticalMaxDeg),
                            params.verticalMaxDeg);
    params.verticalStepDeg =
        SanitizePositiveFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarVerticalStepDeg, params.verticalStepDeg),
                              params.verticalStepDeg, 1.0e-4f);
    params.pointRadiusPixels =
        SanitizePositiveFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarPointRadiusPixels,
                                                params.pointRadiusPixels),
                              params.pointRadiusPixels, 0.0f);
    params.maxDistance =
        SanitizePositiveFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->lidarMaxDistance, params.maxDistance),
                              params.maxDistance, 1.0e-3f);
    return params;
  }

  HdRobotHeightScanParams ReadHeightScanParams(HdSceneDelegate* sceneDelegate, const SdfPath& id)
  {
    HdRobotHeightScanParams params;
    params.minX = SanitizeFiniteFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMinX, params.minX), params.minX);
    params.maxX = SanitizeFiniteFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMaxX, params.maxX), params.maxX);
    params.stepX = SanitizePositiveFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanStepX, params.stepX), params.stepX,
        1.0e-4f);
    params.minZ = SanitizeFiniteFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMinZ, params.minZ), params.minZ);
    params.maxZ = SanitizeFiniteFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMaxZ, params.maxZ), params.maxZ);
    params.stepZ = SanitizePositiveFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanStepZ, params.stepZ), params.stepZ,
        1.0e-4f);
    params.rayDirection =
        SanitizeDirection(ReadOptionalVec3(sceneDelegate, id, HdRobotCameraParamTokens->heightScanRayDirection,
                                           params.rayDirection),
                          params.rayDirection);
    params.pointRadiusPixels =
        SanitizePositiveFloat(ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanPointRadiusPixels,
                                                params.pointRadiusPixels),
                              params.pointRadiusPixels, 0.0f);
    params.maxDistance = SanitizePositiveFloat(
        ReadOptionalFloat(sceneDelegate, id, HdRobotCameraParamTokens->heightScanMaxDistance, params.maxDistance),
        params.maxDistance, 1.0e-3f);
    return params;
  }

  HdRobotLidarData BuildLidarData(const HdRobotCameraData& cameraData, const HdRobotLidarParams& params)
  {
    HdRobotLidarData lidarData;
    lidarData.name   = cameraData.name;
    lidarData.eye    = cameraData.position;
    lidarData.center = cameraData.position + cameraData.forward;
    lidarData.up     = cameraData.up;
    lidarData.fovDeg = cameraData.vfov_deg;
    lidarData.params = params;
    return lidarData;
  }

  HdRobotHeightScanData BuildHeightScanData(const HdRobotCameraData& cameraData,
                                            const HdRobotHeightScanParams& params)
  {
    HdRobotHeightScanData heightScanData;
    heightScanData.name   = cameraData.name;
    heightScanData.eye    = cameraData.position;
    heightScanData.center = cameraData.position + cameraData.forward;
    heightScanData.up     = cameraData.up;
    heightScanData.fovDeg = cameraData.vfov_deg;
    heightScanData.params = params;
    return heightScanData;
  }
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

const HdRobotCameraData &HdRobotCamera::GetCameraData() const
{
  return _cameraData;
}

void HdRobotCamera::Finalize(HdRenderParam* renderParam)
{
  if(auto* robotRenderParam = dynamic_cast<HdRobotRenderParam*>(renderParam))
  {
    robotRenderParam->ClearLidarCamera(GetId());
    robotRenderParam->ClearHeightScanCamera(GetId());
  }
  HdCamera::Finalize(renderParam);
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
  cameraData.name = GetId().GetString();
  cameraData.isLidarCamera =
      ReadOptionalBool(sceneDelegate, GetId(), HdRobotCameraParamTokens->lidarIsLidar, false);
  cameraData.isHeightScanCamera =
      ReadOptionalBool(sceneDelegate, GetId(), HdRobotCameraParamTokens->heightScanIsHeightScan, false);
  _cameraData = cameraData;

  if(auto* robotRenderParam = dynamic_cast<HdRobotRenderParam*>(renderParam))
  {
    if(cameraData.isLidarCamera)
    {
      robotRenderParam->UpdateLidarCamera(GetId(), BuildLidarData(cameraData, ReadLidarParams(sceneDelegate, GetId())));
    }
    else
    {
      robotRenderParam->ClearLidarCamera(GetId());
    }

    if(cameraData.isHeightScanCamera)
    {
      robotRenderParam->UpdateHeightScanCamera(GetId(),
                                               BuildHeightScanData(cameraData, ReadHeightScanParams(sceneDelegate, GetId())));
    }
    else
    {
      robotRenderParam->ClearHeightScanCamera(GetId());
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
