#pragma once

#include "training_scene.h"
#include "usd_texture_registry.h"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdLux/domeLight.h>

namespace headless_training
{

Light MakeDefaultLight();
glm::vec3 ReadLightBaseEmission(const PXR_NS::UsdPrim& prim, float normalizeFactor, PXR_NS::UsdTimeCode time);
Light ReadDomeLight(const PXR_NS::UsdLuxDomeLight& domeLight,
                    TextureRegistry& textureRegistry,
                    PXR_NS::UsdTimeCode time);
Light ReadSphereLikeLight(const PXR_NS::UsdPrim& prim,
                          float radius,
                          TextureRegistry& textureRegistry,
                          PXR_NS::UsdTimeCode time);

} // namespace headless_training
