#pragma once

#include "training_scene.h"
#include "usd_texture_registry.h"

#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/shader.h>

namespace headless_training
{

int ReadTextureInput(const PXR_NS::UsdShadeShader& shader,
                     const PXR_NS::TfToken& inputName,
                     TextureUsage usage,
                     TextureRegistry& textureRegistry,
                     PXR_NS::UsdTimeCode time);
Material ReadPreviewSurfaceMaterial(const PXR_NS::UsdShadeShader& shader,
                                    TextureRegistry& textureRegistry,
                                    PXR_NS::UsdTimeCode time);
Material ReadMeshMaterial(const PXR_NS::UsdGeomMesh& mesh, TextureRegistry& textureRegistry, PXR_NS::UsdTimeCode time);

} // namespace headless_training
