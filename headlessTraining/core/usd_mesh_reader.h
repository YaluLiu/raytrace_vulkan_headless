#pragma once

#include "training_scene.h"
#include "usd_texture_registry.h"

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <optional>

namespace headless_training
{

struct TexCoordPrimvar
{
  PXR_NS::VtVec2fArray values;
  PXR_NS::TfToken interpolation;
};

std::optional<TexCoordPrimvar> ReadTexCoordPrimvar(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time);
MeshGeometry ReadMeshGeometry(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time);
TrainingMeshInstance ReadMesh(const PXR_NS::UsdGeomMesh& mesh,
                              TextureRegistry& textureRegistry,
                              PXR_NS::UsdTimeCode time);

} // namespace headless_training
