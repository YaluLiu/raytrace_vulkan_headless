#include "usd_scene_reader_utils.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace headless_training
{

glm::vec3 ToGlm(const PXR_NS::GfVec3f& value)
{
  return glm::vec3(value[0], value[1], value[2]);
}

glm::vec3 ToGlm(const PXR_NS::GfVec3d& value)
{
  return glm::vec3(static_cast<float>(value[0]), static_cast<float>(value[1]), static_cast<float>(value[2]));
}

glm::vec4 ToGlm(const PXR_NS::GfQuatd& value)
{
  const PXR_NS::GfVec3d imaginary = value.GetImaginary();
  return glm::vec4(static_cast<float>(imaginary[0]), static_cast<float>(imaginary[1]), static_cast<float>(imaginary[2]),
                   static_cast<float>(value.GetReal()));
}

std::array<double, 16> ToRowMajorArray(const PXR_NS::GfMatrix4d& matrix)
{
  std::array<double, 16> values{};
  for(int row = 0; row < 4; ++row)
  {
    for(int column = 0; column < 4; ++column)
    {
      values[static_cast<size_t>(row * 4 + column)] = matrix[row][column];
    }
  }
  return values;
}

glm::mat4 ToEngineTransform(const PXR_NS::GfMatrix4d& matrix)
{
  return MakeEngineTransformFromUsdRows(ToRowMajorArray(matrix));
}

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback)
{
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
     glm::length2(value) < kSensorEpsilon * kSensorEpsilon)
  {
    value = fallback;
  }
  if(glm::length2(value) < kSensorEpsilon * kSensorEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

CameraSpec MakePoseFromTransform(const std::string& name, const PXR_NS::GfMatrix4d& transform)
{
  const PXR_NS::GfVec3d position = transform.Transform(PXR_NS::GfVec3d(0.0, 0.0, 0.0));
  PXR_NS::GfVec3d forward = transform.TransformDir(PXR_NS::GfVec3d(0.0, 0.0, -1.0));
  PXR_NS::GfVec3d up = transform.TransformDir(PXR_NS::GfVec3d(0.0, 1.0, 0.0));
  forward.Normalize();
  up.Normalize();

  CameraSpec spec;
  spec.name = name;
  spec.position = ToGlm(position);
  spec.forward = NormalizeOr(ToGlm(forward), glm::vec3(0.0f, 0.0f, -1.0f));
  spec.up = NormalizeOr(ToGlm(up), glm::vec3(0.0f, 1.0f, 0.0f));
  if(glm::length2(glm::cross(spec.forward, spec.up)) < kSensorEpsilon * kSensorEpsilon)
  {
    spec.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return spec;
}

float ClampPositive(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, kParamEpsilon);
}

float ClampNonNegative(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, 0.0f);
}

float ClampStep(float value, float fallback)
{
  return std::max(std::fabs(std::isfinite(value) ? value : fallback), kParamEpsilon);
}

bool IsInvisible(const PXR_NS::UsdPrim& prim, PXR_NS::UsdTimeCode time)
{
  PXR_NS::UsdGeomImageable imageable(prim);
  if(!imageable)
  {
    return false;
  }
  return imageable.ComputeVisibility(time) == PXR_NS::UsdGeomTokens->invisible;
}

float ReadFloatAttr(const PXR_NS::UsdPrim& prim, const PXR_NS::TfToken& name, float fallback, PXR_NS::UsdTimeCode time)
{
  const PXR_NS::UsdAttribute attr = prim.GetAttribute(name);
  if(!attr)
  {
    return fallback;
  }

  PXR_NS::VtValue boxed;
  if(!attr.Get(&boxed, time))
  {
    return fallback;
  }

  float value = fallback;
  if(boxed.IsHolding<float>())
  {
    value = boxed.UncheckedGet<float>();
  }
  else if(boxed.IsHolding<double>())
  {
    value = static_cast<float>(boxed.UncheckedGet<double>());
  }
  else if(boxed.IsHolding<int>())
  {
    value = static_cast<float>(boxed.UncheckedGet<int>());
  }
  return std::isfinite(value) ? value : fallback;
}

bool ReadBoolAttr(const PXR_NS::UsdPrim& prim, const PXR_NS::TfToken& name, bool fallback, PXR_NS::UsdTimeCode time)
{
  bool value = fallback;
  const PXR_NS::UsdAttribute attr = prim.GetAttribute(name);
  if(attr)
  {
    attr.Get(&value, time);
  }
  return value;
}

glm::vec3 ReadVec3Attr(const PXR_NS::UsdPrim& prim, const PXR_NS::TfToken& name, glm::vec3 fallback,
                       PXR_NS::UsdTimeCode time)
{
  PXR_NS::GfVec3f value(fallback.x, fallback.y, fallback.z);
  const PXR_NS::UsdAttribute attr = prim.GetAttribute(name);
  if(attr)
  {
    attr.Get(&value, time);
  }
  return ToGlm(value);
}

uint32_t ReadTraceMask(const PXR_NS::UsdPrim& prim)
{
  const PXR_NS::UsdAttribute attr = prim.GetAttribute(PXR_NS::TfToken("hdRobot:traceRole"));
  if(!attr)
  {
    return kTraceMaskDefaultGeometry;
  }

  PXR_NS::VtValue value;
  if(!attr.Get(&value))
  {
    return kTraceMaskDefaultGeometry;
  }

  if(value.IsHolding<PXR_NS::TfToken>() && value.UncheckedGet<PXR_NS::TfToken>() == PXR_NS::TfToken("ground"))
  {
    return kTraceMaskGround;
  }
  if(value.IsHolding<std::string>() && value.UncheckedGet<std::string>() == "ground")
  {
    return kTraceMaskGround;
  }

  return kTraceMaskDefaultGeometry;
}

CameraSpec ReadCamera(const PXR_NS::UsdGeomCamera& camera, PXR_NS::UsdTimeCode time)
{
  CameraSpec result =
      MakePoseFromTransform(camera.GetPath().GetString(),
                            PXR_NS::UsdGeomXformable(camera.GetPrim()).ComputeLocalToWorldTransform(time));

  const PXR_NS::GfCamera gfCamera = camera.GetCamera(time);
  if(gfCamera.GetProjection() == PXR_NS::GfCamera::Perspective && gfCamera.GetFocalLength() > kSensorEpsilon)
  {
    const float verticalAperture = gfCamera.GetVerticalAperture() * PXR_NS::GfCamera::APERTURE_UNIT;
    const float horizontalAperture = gfCamera.GetHorizontalAperture() * PXR_NS::GfCamera::APERTURE_UNIT;
    const float focalLength = gfCamera.GetFocalLength() * PXR_NS::GfCamera::FOCAL_LENGTH_UNIT;
    const float verticalFov = 2.0f * std::atan(verticalAperture / (2.0f * focalLength));
    const float horizontalFov = 2.0f * std::atan(horizontalAperture / (2.0f * focalLength));
    result.verticalFovDegrees = std::clamp(glm::degrees(verticalFov), 1.0f, 179.0f);
    result.horizontalFovDegrees = std::clamp(glm::degrees(horizontalFov), 1.0f, 179.0f);
  }

  const PXR_NS::GfRange1f clippingRange = gfCamera.GetClippingRange();
  result.clipStart = std::max(clippingRange.GetMin(), 0.001f);
  result.clipEnd = std::max(clippingRange.GetMax(), result.clipStart + 0.001f);
  return result;
}

} // namespace headless_training
