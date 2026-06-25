#include "usd_light_reader.h"

#include "usd_scene_reader_utils.h"

#include <pxr/base/tf/token.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/lightAPI.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

namespace headless_training
{
namespace
{
constexpr int kLightTypeSphere = 0;
constexpr int kLightTypeDome = 2;
} // namespace

Light MakeDefaultLight()
{
  Light light{};
  light.textureID = -1;
  light.diffuse = 1.0f;
  light.specular = 1.0f;
  light.radius = 0.5f;
  light.rotateQuat = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  return light;
}

glm::vec3 ReadLightBaseEmission(const PXR_NS::UsdPrim& prim, float normalizeFactor, PXR_NS::UsdTimeCode time)
{
  float intensity = 1.0f;
  float exposure = 0.0f;
  PXR_NS::GfVec3f color(1.0f, 1.0f, 1.0f);

  const PXR_NS::UsdLuxLightAPI lightApi(prim);
  if(lightApi)
  {
    lightApi.GetIntensityAttr().Get(&intensity, time);
    lightApi.GetExposureAttr().Get(&exposure, time);
    lightApi.GetColorAttr().Get(&color, time);
  }
  else
  {
    intensity = ReadFloatAttr(prim, PXR_NS::TfToken("inputs:intensity"), intensity, time);
    exposure = ReadFloatAttr(prim, PXR_NS::TfToken("inputs:exposure"), exposure, time);
    const glm::vec3 fallbackColor = ReadVec3Attr(prim, PXR_NS::TfToken("inputs:color"), glm::vec3(1.0f), time);
    color = PXR_NS::GfVec3f(fallbackColor.x, fallbackColor.y, fallbackColor.z);
  }

  return ToGlm(color) * (intensity * std::pow(2.0f, exposure) / std::max(normalizeFactor, kSensorEpsilon));
}

Light ReadDomeLight(const PXR_NS::UsdLuxDomeLight& domeLight, TextureRegistry& textureRegistry,
                    PXR_NS::UsdTimeCode time)
{
  Light result = MakeDefaultLight();
  result.type = kLightTypeDome;
  result.baseEmission = ReadLightBaseEmission(domeLight.GetPrim(), 1.0f, time);
  result.diffuse = ReadFloatAttr(domeLight.GetPrim(), PXR_NS::TfToken("inputs:diffuse"), 1.0f, time);
  result.specular = ReadFloatAttr(domeLight.GetPrim(), PXR_NS::TfToken("inputs:specular"), 1.0f, time);
  result.rotateQuat = ToGlm(PXR_NS::UsdGeomXformable(domeLight.GetPrim())
                                .ComputeLocalToWorldTransform(time)
                                .GetOrthonormalized()
                                .ExtractRotationQuat());

  PXR_NS::SdfAssetPath texturePath;
  if(domeLight.GetTextureFileAttr().Get(&texturePath, time))
  {
    result.textureID = textureRegistry.Register(texturePath, TextureUsage::Light);
  }
  return result;
}

Light ReadSphereLikeLight(const PXR_NS::UsdPrim& prim, float radius, TextureRegistry& textureRegistry,
                          PXR_NS::UsdTimeCode time)
{
  (void)textureRegistry;
  Light result = MakeDefaultLight();
  result.type = kLightTypeSphere;
  result.position = ToGlm(
      PXR_NS::UsdGeomXformable(prim).ComputeLocalToWorldTransform(time).Transform(PXR_NS::GfVec3d(0.0, 0.0, 0.0)));
  result.radius = std::max(radius, 0.0f);
  result.diffuse = ReadFloatAttr(prim, PXR_NS::TfToken("inputs:diffuse"), 1.0f, time);
  result.specular = ReadFloatAttr(prim, PXR_NS::TfToken("inputs:specular"), 1.0f, time);

  const bool normalize = ReadBoolAttr(prim, PXR_NS::TfToken("inputs:normalize"), false, time);
  const float area = 4.0f * glm::pi<float>() * result.radius * result.radius;
  const float normalizeFactor = normalize && area > kSensorEpsilon ? area : 1.0f;
  result.baseEmission = ReadLightBaseEmission(prim, normalizeFactor, time);
  return result;
}

} // namespace headless_training
