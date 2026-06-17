#include "usd_scene_loader.h"

#include "UsdRaySensor/heightScanSensor.h"
#include "UsdRaySensor/lidarSensor.h"
#include "UsdRaySensor/tokens.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolverContextBinder.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace headless_training
{
namespace
{
constexpr float kSensorEpsilon = 1.0e-6f;
constexpr float kParamEpsilon = 1.0e-4f;
constexpr int kLightTypeSphere = 0;
constexpr int kLightTypeDome = 2;

std::string TextureRegistryKey(const std::string& texturePath, TextureUsage usage)
{
  return texturePath + "#" + std::to_string(static_cast<int>(usage));
}

TextureColorSpace TextureColorSpaceForUsage(TextureUsage usage)
{
  switch(usage)
  {
  case TextureUsage::Metallic:
  case TextureUsage::Roughness:
  case TextureUsage::Normal:
  case TextureUsage::Opacity:
  case TextureUsage::Subsurface:
    return TextureColorSpace::Linear;
  case TextureUsage::Unknown:
  case TextureUsage::BaseColor:
  case TextureUsage::Emission:
  case TextureUsage::Light:
  default:
    return TextureColorSpace::SRGB;
  }
}

class TextureRegistry
{
public:
  explicit TextureRegistry(PXR_NS::UsdStageRefPtr stage) : m_stage(std::move(stage)) {}

  int Register(const PXR_NS::SdfAssetPath& assetPath, TextureUsage usage)
  {
    const std::string authoredPath = assetPath.GetAssetPath();
    const std::string resolvedPath = assetPath.GetResolvedPath();
    const std::string sourcePath = !resolvedPath.empty() ? resolvedPath : authoredPath;
    if(sourcePath.empty())
    {
      return -1;
    }

    const std::string key = TextureRegistryKey(sourcePath, usage);
    const auto it = m_textureIdByKey.find(key);
    if(it != m_textureIdByKey.end())
    {
      return it->second;
    }

    TextureAsset textureAsset;
    textureAsset.sourcePath = sourcePath;
    textureAsset.usage = usage;
    textureAsset.colorSpace = TextureColorSpaceForUsage(usage);
    ExportTextureBytes(assetPath, textureAsset);

    const int textureId = static_cast<int>(m_textureAssets.size());
    m_textureAssets.emplace_back(std::move(textureAsset));
    m_textureIdByKey.emplace(key, textureId);
    return textureId;
  }

  std::vector<TextureAsset> TakeAssets()
  {
    return std::move(m_textureAssets);
  }

private:
  PXR_NS::UsdStageRefPtr m_stage;
  std::unordered_map<std::string, int> m_textureIdByKey;
  std::vector<TextureAsset> m_textureAssets;

  void ExportTextureBytes(const PXR_NS::SdfAssetPath& assetPath, TextureAsset& textureAsset) const
  {
    PXR_NS::ArResolverContextBinder contextBinder(m_stage->GetPathResolverContext());
    PXR_NS::ArResolver& resolver = PXR_NS::ArGetResolver();

    PXR_NS::ArResolvedPath resolvedPath;
    if(!assetPath.GetResolvedPath().empty())
    {
      resolvedPath = PXR_NS::ArResolvedPath(assetPath.GetResolvedPath());
    }
    if(!resolvedPath)
    {
      resolvedPath = resolver.Resolve(assetPath.GetAssetPath());
    }
    if(!resolvedPath)
    {
      std::cerr << "[robot_training_headless] Warning: failed to resolve "
                   "texture asset "
                << assetPath.GetAssetPath() << '\n';
      return;
    }

    textureAsset.sourcePath = resolvedPath.GetPathString();
    std::shared_ptr<PXR_NS::ArAsset> asset = resolver.OpenAsset(resolvedPath);
    if(!asset)
    {
      std::cerr << "[robot_training_headless] Warning: failed to open texture asset " << resolvedPath.GetPathString()
                << '\n';
      return;
    }

    const auto buffer = asset->GetBuffer();
    const auto size = asset->GetSize();
    if(!buffer || size == 0)
    {
      std::cerr << "[robot_training_headless] Warning: empty texture asset " << resolvedPath.GetPathString() << '\n';
      return;
    }

    const auto* bytes = reinterpret_cast<const uint8_t*>(buffer.get());
    textureAsset.encodedBytes.assign(bytes, bytes + size);
  }
};

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

LidarParams SanitizeLidarParams(LidarParams params)
{
  LidarParams defaults;
  params.azimuthStepDeg = ClampStep(params.azimuthStepDeg, defaults.azimuthStepDeg);
  params.verticalStepDeg = ClampStep(params.verticalStepDeg, defaults.verticalStepDeg);
  params.maxRange = ClampPositive(params.maxRange, defaults.maxRange);
  params.intensity = ClampNonNegative(params.intensity, defaults.intensity);
  return params;
}

HeightScanParams SanitizeHeightScanParams(HeightScanParams params)
{
  HeightScanParams defaults;
  params.uStep = ClampStep(params.uStep, defaults.uStep);
  params.vStep = ClampStep(params.vStep, defaults.vStep);
  params.gravityDirectionWs = NormalizeOr(params.gravityDirectionWs, defaults.gravityDirectionWs);
  params.maxRange = ClampPositive(params.maxRange, defaults.maxRange);
  return params;
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

bool ReadFloatAttr(const PXR_NS::UsdPrim& prim, const PXR_NS::TfToken& name, float fallback, PXR_NS::UsdTimeCode time)
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

glm::vec3 ComputeTriangleNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
  const glm::vec3 normal = glm::cross(b - a, c - a);
  if(glm::length2(normal) < kSensorEpsilon * kSensorEpsilon)
  {
    return glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return glm::normalize(normal);
}

glm::vec2 DefaultTexCoord(glm::vec3 position)
{
  return glm::vec2((position.x + 1.0f) * 0.5f, 1.0f - ((position.z + 1.0f) * 0.5f));
}

void AppendTriangle(MeshGeometry& geometry,
                    const PXR_NS::VtVec3fArray& points,
                    const std::array<glm::vec3, 3>& cornerNormals,
                    int i0,
                    int i1,
                    int i2)
{
  const glm::vec3 p0 = ToGlm(points[static_cast<size_t>(i0)]);
  const glm::vec3 p1 = ToGlm(points[static_cast<size_t>(i1)]);
  const glm::vec3 p2 = ToGlm(points[static_cast<size_t>(i2)]);
  const glm::vec3 faceNormal = ComputeTriangleNormal(p0, p1, p2);
  const int indices[3] = {i0, i1, i2};
  const glm::vec3 positions[3] = {p0, p1, p2};

  for(int corner = 0; corner < 3; ++corner)
  {
    MeshVertex vertex;
    vertex.pos = positions[corner];
    vertex.nrm = NormalizeOr(cornerNormals[static_cast<size_t>(corner)], faceNormal);
    vertex.color = glm::vec3(1.0f);
    vertex.texCoord = DefaultTexCoord(vertex.pos);
    geometry.indices.push_back(static_cast<uint32_t>(geometry.vertices.size()));
    geometry.vertices.push_back(vertex);
  }
  geometry.materialIndices.push_back(0);
}

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

Material ReadPreviewSurfaceMaterial(const PXR_NS::UsdShadeShader& shader, TextureRegistry& textureRegistry,
                                    PXR_NS::UsdTimeCode time)
{
  Material material;

  glm::vec3 color;
  const bool hasBaseColor = ReadVec3Input(shader, PXR_NS::TfToken("diffuseColor"), time, &color);
  if(hasBaseColor)
  {
    material.diffuse = color;
    material.baseColorFactor = color;
    material.ambient = color * 0.1f;
  }

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

  const int baseColorTextureId =
      ReadTextureInput(shader, PXR_NS::TfToken("diffuseColor"), TextureUsage::BaseColor, textureRegistry, time);
  if(baseColorTextureId >= 0)
  {
    material.baseColorTextureId = baseColorTextureId;
    if(!hasBaseColor)
    {
      material.diffuse = glm::vec3(1.0f);
      material.baseColorFactor = glm::vec3(1.0f);
      material.ambient = glm::vec3(0.1f);
    }
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

std::optional<glm::vec3> ReadAuthoredNormal(const PXR_NS::VtVec3fArray& normals,
                                            const PXR_NS::TfToken& interpolation,
                                            int pointIndex,
                                            size_t faceIndex,
                                            int faceVertexStart,
                                            int faceCornerIndex)
{
  if(normals.empty())
  {
    return std::nullopt;
  }

  size_t normalIndex = std::numeric_limits<size_t>::max();
  if(interpolation == PXR_NS::UsdGeomTokens->faceVarying)
  {
    normalIndex = static_cast<size_t>(faceVertexStart + faceCornerIndex);
  }
  else if(interpolation == PXR_NS::UsdGeomTokens->uniform)
  {
    normalIndex = faceIndex;
  }
  else if(interpolation == PXR_NS::UsdGeomTokens->constant)
  {
    normalIndex = 0;
  }
  else
  {
    normalIndex = static_cast<size_t>(pointIndex);
  }

  if(normalIndex >= normals.size())
  {
    return std::nullopt;
  }
  return ToGlm(normals[normalIndex]);
}

std::array<glm::vec3, 3> ResolveTriangleNormals(const PXR_NS::VtVec3fArray& points,
                                                const PXR_NS::VtVec3fArray& normals,
                                                const PXR_NS::TfToken& interpolation,
                                                size_t faceIndex,
                                                int faceVertexStart,
                                                const int pointIndices[3],
                                                const int faceCornerIndices[3])
{
  const glm::vec3 p0 = ToGlm(points[static_cast<size_t>(pointIndices[0])]);
  const glm::vec3 p1 = ToGlm(points[static_cast<size_t>(pointIndices[1])]);
  const glm::vec3 p2 = ToGlm(points[static_cast<size_t>(pointIndices[2])]);
  const glm::vec3 fallback = ComputeTriangleNormal(p0, p1, p2);

  std::array<glm::vec3, 3> result{fallback, fallback, fallback};
  for(size_t corner = 0; corner < result.size(); ++corner)
  {
    const std::optional<glm::vec3> authored =
        ReadAuthoredNormal(normals, interpolation, pointIndices[corner], faceIndex, faceVertexStart,
                           faceCornerIndices[corner]);
    if(authored)
    {
      result[corner] = *authored;
    }
  }
  return result;
}

MeshGeometry ReadMeshGeometry(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time)
{
  PXR_NS::VtVec3fArray points;
  PXR_NS::VtIntArray faceVertexCounts;
  PXR_NS::VtIntArray faceVertexIndices;
  PXR_NS::VtVec3fArray normals;

  if(!mesh.GetPointsAttr().Get(&points, time) || points.empty())
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has no points");
  }
  if(!mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts, time) || faceVertexCounts.empty())
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has no faceVertexCounts");
  }
  if(!mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices, time) || faceVertexIndices.empty())
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has no faceVertexIndices");
  }
  mesh.GetNormalsAttr().Get(&normals, time);
  const PXR_NS::TfToken normalsInterpolation = mesh.GetNormalsInterpolation();

  MeshGeometry geometry;
  int indexCursor = 0;
  for(size_t faceIndex = 0; faceIndex < faceVertexCounts.size(); ++faceIndex)
  {
    const int count = faceVertexCounts[faceIndex];
    if(count < 3)
    {
      throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " contains a face with fewer than 3 vertices");
    }
    if(indexCursor + count > static_cast<int>(faceVertexIndices.size()))
    {
      throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " faceVertexIndices are shorter than counts");
    }
    if(count > 4)
    {
      std::cerr << "[robot_training_headless] Warning: fan triangulating n-gon face " << faceIndex << " on "
                << mesh.GetPath().GetString() << "; only simple convex faces are supported\n";
    }

    for(int i = 1; i + 1 < count; ++i)
    {
      const int tri[3] = {
          faceVertexIndices[static_cast<size_t>(indexCursor)],
          faceVertexIndices[static_cast<size_t>(indexCursor + i)],
          faceVertexIndices[static_cast<size_t>(indexCursor + i + 1)],
      };
      const int faceCornerIndices[3] = {0, i, i + 1};
      for(const int pointIndex : tri)
      {
        if(pointIndex < 0 || pointIndex >= static_cast<int>(points.size()))
        {
          throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has out-of-range point index");
        }
      }
      const std::array<glm::vec3, 3> cornerNormals =
          ResolveTriangleNormals(points, normals, normalsInterpolation, faceIndex, indexCursor, tri, faceCornerIndices);
      AppendTriangle(geometry, points, cornerNormals, tri[0], tri[1], tri[2]);
    }
    indexCursor += count;
  }
  if(indexCursor != static_cast<int>(faceVertexIndices.size()))
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has unused faceVertexIndices");
  }
  return geometry;
}

TrainingMeshInstance ReadMesh(const PXR_NS::UsdGeomMesh& mesh, TextureRegistry& textureRegistry,
                              PXR_NS::UsdTimeCode time)
{
  TrainingMeshInstance result;
  result.name = mesh.GetPath().GetString();
  result.geometry = ReadMeshGeometry(mesh, time);
  result.materials.push_back(ReadMeshMaterial(mesh, textureRegistry, time));
  result.worldTransform = ToEngineTransform(PXR_NS::UsdGeomXformable(mesh.GetPrim()).ComputeLocalToWorldTransform(time));
  result.visible = !IsInvisible(mesh.GetPrim(), time);
  result.traceMask = ReadTraceMask(mesh.GetPrim());
  return result;
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

bool IsLidarSensorPrim(const PXR_NS::UsdPrim& prim)
{
  return prim.GetTypeName() == PXR_NS::UsdRaySensorTokens->LidarSensor || prim.IsA<PXR_NS::UsdGeomLidarSensor>();
}

bool IsHeightScanSensorPrim(const PXR_NS::UsdPrim& prim)
{
  return prim.GetTypeName() == PXR_NS::UsdRaySensorTokens->HeightScanSensor ||
         prim.IsA<PXR_NS::UsdGeomHeightScanSensor>();
}

LidarSensorSpec ReadLidarSensor(const PXR_NS::UsdGeomLidarSensor& sensor, PXR_NS::UsdTimeCode time)
{
  CameraSpec pose =
      MakePoseFromTransform(sensor.GetPath().GetString(),
                            PXR_NS::UsdGeomXformable(sensor.GetPrim()).ComputeLocalToWorldTransform(time));
  LidarSensorSpec result;
  result.name = pose.name;
  result.position = pose.position;
  result.forward = pose.forward;
  result.up = pose.up;
  result.params.azimuthStartDeg = sensor.GetAzimuthStartDeg(time);
  result.params.azimuthEndDeg = sensor.GetAzimuthEndDeg(time);
  result.params.azimuthStepDeg = sensor.GetAzimuthStepDeg(time);
  result.params.verticalStartDeg = sensor.GetVerticalStartDeg(time);
  result.params.verticalEndDeg = sensor.GetVerticalEndDeg(time);
  result.params.verticalStepDeg = sensor.GetVerticalStepDeg(time);
  result.params.maxRange = sensor.GetMaxRange(time);
  result.params.intensity = sensor.GetIntensity(time);
  result.params = SanitizeLidarParams(result.params);
  return result;
}

HeightScanSensorSpec ReadHeightScanSensor(const PXR_NS::UsdGeomHeightScanSensor& sensor, PXR_NS::UsdTimeCode time)
{
  CameraSpec pose =
      MakePoseFromTransform(sensor.GetPath().GetString(),
                            PXR_NS::UsdGeomXformable(sensor.GetPrim()).ComputeLocalToWorldTransform(time));
  HeightScanSensorSpec result;
  result.name = pose.name;
  result.position = pose.position;
  result.forward = pose.forward;
  result.up = pose.up;
  result.params.uStart = sensor.GetUStart(time);
  result.params.uEnd = sensor.GetUEnd(time);
  result.params.uStep = sensor.GetUStep(time);
  result.params.vStart = sensor.GetVStart(time);
  result.params.vEnd = sensor.GetVEnd(time);
  result.params.vStep = sensor.GetVStep(time);
  result.params.gravityDirectionWs = ToGlm(sensor.GetGravityDirectionWs(time));
  result.params.maxRange = sensor.GetMaxRange(time);
  result.params = SanitizeHeightScanParams(result.params);
  return result;
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
} // namespace

TrainingSceneDescription LoadUsdTrainingScene(const std::filesystem::path& usdPath, const UsdSceneLoadOptions& options)
{
  PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(usdPath.string());
  if(!stage)
  {
    throw std::runtime_error("failed to open USD stage: " + usdPath.string());
  }

  const PXR_NS::UsdTimeCode time = PXR_NS::UsdTimeCode::Default();
  TrainingSceneDescription result;
  TextureRegistry textureRegistry(stage);
  bool cameraPathRequested = !options.cameraPath.empty();
  bool requestedCameraFound = false;

  for(const PXR_NS::UsdPrim& prim : stage->Traverse())
  {
    if(!prim.IsActive())
    {
      continue;
    }

    PXR_NS::UsdGeomMesh mesh(prim);
    if(mesh)
    {
      result.meshes.push_back(ReadMesh(mesh, textureRegistry, time));
      continue;
    }

    PXR_NS::UsdGeomCamera camera(prim);
    if(camera)
    {
      const bool selected = !cameraPathRequested || prim.GetPath() == PXR_NS::SdfPath(options.cameraPath);
      if(selected && result.cameras.empty())
      {
        result.cameras.push_back(ReadCamera(camera, time));
        requestedCameraFound = cameraPathRequested;
      }
      continue;
    }

    if(IsLidarSensorPrim(prim))
    {
      PXR_NS::UsdGeomLidarSensor sensor(prim);
      if(sensor.GetEnabled(time))
      {
        result.lidarSensors.push_back(ReadLidarSensor(sensor, time));
      }
      continue;
    }

    if(IsHeightScanSensorPrim(prim))
    {
      PXR_NS::UsdGeomHeightScanSensor sensor(prim);
      if(sensor.GetEnabled(time))
      {
        result.heightScanSensors.push_back(ReadHeightScanSensor(sensor, time));
      }
      continue;
    }

    PXR_NS::UsdLuxDomeLight domeLight(prim);
    if(domeLight)
    {
      result.lights.push_back(ReadDomeLight(domeLight, textureRegistry, time));
      continue;
    }

    PXR_NS::UsdLuxSphereLight sphereLight(prim);
    if(sphereLight)
    {
      float radius = 0.5f;
      sphereLight.GetRadiusAttr().Get(&radius, time);
      result.lights.push_back(ReadSphereLikeLight(prim, radius, textureRegistry, time));
      continue;
    }

    PXR_NS::UsdLuxCylinderLight cylinderLight(prim);
    if(cylinderLight)
    {
      float radius = 0.5f;
      cylinderLight.GetRadiusAttr().Get(&radius, time);
      result.lights.push_back(ReadSphereLikeLight(prim, radius, textureRegistry, time));
      continue;
    }
  }

  if(cameraPathRequested && !requestedCameraFound)
  {
    throw std::runtime_error("requested camera path was not found or was not a UsdGeomCamera: " + options.cameraPath);
  }

  result.textureAssets = textureRegistry.TakeAssets();
  return result;
}

} // namespace headless_training
