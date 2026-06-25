#include "usd_material_reader.h"

#include "usd_scene_reader_utils.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/tokens.h>

#include <algorithm>

#include <glm/glm.hpp>

namespace headless_training
{
namespace
{

bool ReadVec3Input(const PXR_NS::UsdShadeShader& shader, const PXR_NS::TfToken& name, PXR_NS::UsdTimeCode time,
                   glm::vec3* value)
{
  const PXR_NS::UsdShadeInput input = shader.GetInput(name);
  if(!input)
  {
    return false;
  }

  PXR_NS::GfVec3f usdValue;
  if(!input.Get(&usdValue, time))
  {
    return false;
  }

  *value = ToGlm(usdValue);
  return true;
}

bool ReadFloatInput(const PXR_NS::UsdShadeShader& shader, const PXR_NS::TfToken& name, PXR_NS::UsdTimeCode time,
                    float* value)
{
  const PXR_NS::UsdShadeInput input = shader.GetInput(name);
  if(!input)
  {
    return false;
  }
  return input.Get(value, time);
}

bool ApplyDisplayColorMaterial(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time, Material* material)
{
  const PXR_NS::UsdGeomGprim gprim(mesh.GetPrim());
  PXR_NS::VtVec3fArray colors;
  if(!gprim.GetDisplayColorAttr().Get(&colors, time) || colors.empty())
  {
    return false;
  }

  float opacity = 1.0f;
  PXR_NS::VtFloatArray opacities;
  if(gprim.GetDisplayOpacityAttr().Get(&opacities, time) && !opacities.empty())
  {
    opacity = std::clamp(opacities[0], 0.0f, 1.0f);
  }

  const glm::vec3 color = ToGlm(colors[0]);
  material->diffuse = color;
  material->baseColorFactor = color;
  material->ambient = color * 0.1f;
  material->opaque = opacity;
  material->opacityFactor = opacity;
  return true;
}

} // namespace

int ReadTextureInput(const PXR_NS::UsdShadeShader& shader, const PXR_NS::TfToken& inputName, TextureUsage usage,
                     TextureRegistry& textureRegistry, PXR_NS::UsdTimeCode time)
{
  const PXR_NS::UsdShadeInput input = shader.GetInput(inputName);
  if(!input)
  {
    return -1;
  }

  PXR_NS::UsdShadeConnectableAPI source;
  PXR_NS::TfToken sourceName;
  PXR_NS::UsdShadeAttributeType sourceType;
  if(!input.GetConnectedSource(&source, &sourceName, &sourceType))
  {
    return -1;
  }

  const PXR_NS::UsdShadeShader textureShader(source.GetPrim());
  if(!textureShader)
  {
    return -1;
  }

  const PXR_NS::UsdShadeInput fileInput = textureShader.GetInput(PXR_NS::TfToken("file"));
  if(!fileInput)
  {
    return -1;
  }

  PXR_NS::SdfAssetPath texturePath;
  if(!fileInput.Get(&texturePath, time))
  {
    return -1;
  }
  return textureRegistry.Register(texturePath, usage);
}

Material ReadPreviewSurfaceMaterial(const PXR_NS::UsdShadeShader& shader, TextureRegistry& textureRegistry,
                                    PXR_NS::UsdTimeCode time)
{
  Material material;

  const int baseColorTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("diffuseColor"), TextureUsage::BaseColor, textureRegistry, time);
  if(baseColorTextureId >= 0)
  {
    material.baseColorTextureId = baseColorTextureId;
    material.diffuse = glm::vec3(1.0f);
    material.baseColorFactor = glm::vec3(1.0f);
    material.ambient = glm::vec3(0.1f);
  }
  else
  {
    glm::vec3 color;
    if(ReadVec3Input(shader, PXR_NS::TfToken("diffuseColor"), time, &color))
    {
      material.diffuse = color;
      material.baseColorFactor = color;
      material.ambient = color * 0.1f;
    }
  }

  glm::vec3 color;
  if(ReadVec3Input(shader, PXR_NS::TfToken("emissiveColor"), time, &color))
  {
    material.emission = color;
    material.emissionFactor = color;
  }

  float value = 0.0f;
  if(ReadFloatInput(shader, PXR_NS::TfToken("metallic"), time, &value))
  {
    material.metallicFactor = std::clamp(value, 0.0f, 1.0f);
  }
  if(ReadFloatInput(shader, PXR_NS::TfToken("roughness"), time, &value))
  {
    material.roughnessFactor = std::clamp(value, 0.02f, 1.0f);
  }
  if(ReadFloatInput(shader, PXR_NS::TfToken("opacity"), time, &value))
  {
    material.opaque = std::clamp(value, 0.0f, 1.0f);
    material.opacityFactor = material.opaque;
  }

  material.metallicTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("metallic"), TextureUsage::Metallic, textureRegistry, time);
  material.roughnessTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("roughness"), TextureUsage::Roughness, textureRegistry, time);
  material.normalTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("normal"), TextureUsage::Normal, textureRegistry, time);
  material.emissionTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("emissiveColor"), TextureUsage::Emission, textureRegistry, time);
  material.opacityTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("opacity"), TextureUsage::Opacity, textureRegistry, time);

  return material;
}

Material ReadMeshMaterial(const PXR_NS::UsdGeomMesh& mesh, TextureRegistry& textureRegistry, PXR_NS::UsdTimeCode time)
{
  PXR_NS::UsdShadeMaterial material =
      PXR_NS::UsdShadeMaterialBindingAPI(mesh.GetPrim()).ComputeBoundMaterial(PXR_NS::UsdShadeTokens->allPurpose);
  if(material)
  {
    PXR_NS::TfToken sourceName;
    PXR_NS::UsdShadeAttributeType sourceType;
    const PXR_NS::UsdShadeShader surfaceShader = material.ComputeSurfaceSource(
        PXR_NS::TfTokenVector{PXR_NS::UsdShadeTokens->universalRenderContext}, &sourceName, &sourceType);
    if(surfaceShader)
    {
      return ReadPreviewSurfaceMaterial(surfaceShader, textureRegistry, time);
    }
  }

  Material fallbackMaterial;
  ApplyDisplayColorMaterial(mesh, time, &fallbackMaterial);
  return fallbackMaterial;
}

} // namespace headless_training
