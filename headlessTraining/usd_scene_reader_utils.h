#pragma once

#include "training_scene.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>

#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace headless_training
{

constexpr float kSensorEpsilon = 1.0e-6f;
constexpr float kParamEpsilon = 1.0e-4f;

glm::vec3 ToGlm(const PXR_NS::GfVec3f& value);
glm::vec3 ToGlm(const PXR_NS::GfVec3d& value);
glm::vec4 ToGlm(const PXR_NS::GfQuatd& value);

std::array<double, 16> ToRowMajorArray(const PXR_NS::GfMatrix4d& matrix);
glm::mat4 ToEngineTransform(const PXR_NS::GfMatrix4d& matrix);

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback);
CameraSpec MakePoseFromTransform(const std::string& name, const PXR_NS::GfMatrix4d& transform);

float ClampPositive(float value, float fallback);
float ClampNonNegative(float value, float fallback);
float ClampStep(float value, float fallback);

bool IsInvisible(const PXR_NS::UsdPrim& prim, PXR_NS::UsdTimeCode time);
float ReadFloatAttr(const PXR_NS::UsdPrim& prim,
                    const PXR_NS::TfToken& name,
                    float fallback,
                    PXR_NS::UsdTimeCode time);
bool ReadBoolAttr(const PXR_NS::UsdPrim& prim,
                  const PXR_NS::TfToken& name,
                  bool fallback,
                  PXR_NS::UsdTimeCode time);
glm::vec3 ReadVec3Attr(const PXR_NS::UsdPrim& prim,
                       const PXR_NS::TfToken& name,
                       glm::vec3 fallback,
                       PXR_NS::UsdTimeCode time);
uint32_t ReadTraceMask(const PXR_NS::UsdPrim& prim);

CameraSpec ReadCamera(const PXR_NS::UsdGeomCamera& camera, PXR_NS::UsdTimeCode time);

} // namespace headless_training
