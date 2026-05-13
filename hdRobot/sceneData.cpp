#include "sceneData.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

void HydraMaterial::set_default()
{
  diffuse = glm::vec3(0.18f, 0.18f, 0.18f);
  baseColorFactor = diffuse;
  emission = glm::vec3(0.0f, 0.0f, 0.0f);
  emissionFactor = emission;
  transmissionColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  subsurfaceColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  metallicFactor = 0.0f;
  roughnessFactor = 0.5f;
  opacityFactor = 1.0f;
  transmissionFactor = 0.0f;
  subsurfaceFactor = 0.0f;
  subsurfaceScale = 0.0f;
  diffuseTextureId = -1;
  baseColorTextureId = -1;
  metallicTextureId = -1;
  roughnessTextureId = -1;
  normalTextureId = -1;
  emissionTextureId = -1;
  opacityTextureId = -1;
  subsurfaceTextureId = -1;
  texturePath.clear();
  material_changed = true;
}

MaterialObj HydraMaterial::toMaterialObj() const
{
  MaterialObj mat;
  mat.ambient = ambient;
  mat.diffuse = diffuse;
  mat.specular = specular;
  mat.transmittance = transmittance;
  mat.emission = emission;
  mat.baseColorFactor = baseColorFactor;
  mat.emissionFactor = emissionFactor;
  mat.transmissionColorFactor = transmissionColorFactor;
  mat.subsurfaceColorFactor = subsurfaceColorFactor;
  mat.shininess = shininess;
  mat.ior = ior;
  mat.opaque = opaque;
  mat.metallicFactor = metallicFactor;
  mat.roughnessFactor = roughnessFactor;
  mat.opacityFactor = opacityFactor;
  mat.transmissionFactor = transmissionFactor;
  mat.subsurfaceFactor = subsurfaceFactor;
  mat.subsurfaceScale = subsurfaceScale;
  mat.illum = illum;
  mat.diffuseTextureId = diffuseTextureId;
  mat.baseColorTextureId = baseColorTextureId;
  mat.metallicTextureId = metallicTextureId;
  mat.roughnessTextureId = roughnessTextureId;
  mat.normalTextureId = normalTextureId;
  mat.emissionTextureId = emissionTextureId;
  mat.opacityTextureId = opacityTextureId;
  mat.subsurfaceTextureId = subsurfaceTextureId;
  return mat;
}

Light HydraLight::toLight() const
{
  Light light;
  light.type = type;
  light.textureID = textureID;
  light.baseEmission = baseEmission;
  light.diffuse = diffuse;
  light.specular = specular;
  light.direction = direction;
  light.angle = angle;
  light.position = position;
  light.radius = radius;
  light.rotateQuat = rotateQuat;
  return light;
}

namespace
{
std::string TextureRegistryKey(const std::string &texturePath, TextureUsage usage)
{
  return texturePath + "#" + std::to_string(static_cast<int>(usage));
}
}  // namespace

TextureColorSpace TextureColorSpaceForUsage(TextureUsage usage)
{
  switch (usage)
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

int TextureRegistry::Register(const std::string &texturePath, TextureUsage usage)
{
  const std::string key = TextureRegistryKey(texturePath, usage);
  const auto it = _textureIdByPath.find(key);
  if (it != _textureIdByPath.end())
  {
    return it->second;
  }

  const int textureId = static_cast<int>(_texturePaths.size());
  _texturePaths.emplace_back(texturePath);
  TextureAsset textureAsset;
  textureAsset.sourcePath = texturePath;
  textureAsset.usage = usage;
  textureAsset.colorSpace = TextureColorSpaceForUsage(usage);
  _textureAssets.emplace_back(std::move(textureAsset));
  _textureIdByPath.emplace(key, textureId);
  ++_version;
  return textureId;
}

const std::vector<std::string> &TextureRegistry::GetPaths() const
{
  return _texturePaths;
}

const std::vector<TextureAsset> &TextureRegistry::GetTextureAssets() const
{
  return _textureAssets;
}

uint64_t TextureRegistry::GetVersion() const
{
  return _version;
}

void ConvertVmeshToLoader(const HydraMesh &mesh, ModelLoader &loader)
{
  const uint32_t vertexOffset = 0;

  loader.m_vertices.clear();
  loader.m_indices.clear();
  loader.m_matIndx.clear();

  const auto &points = mesh.points;
  const auto &normals = mesh.normals;
  const auto &texCoords = mesh.texCoords;
  const auto &tangents = mesh.tangents;
  const auto &bitangentSigns = mesh.bitangentSigns;
  const auto &faces = mesh.faces;
  const auto &matIdx = mesh.materialIds;

  VtVec2fArray newTexCoords = texCoords;
  if (newTexCoords.empty() || newTexCoords.size() != points.size())
  {
    newTexCoords.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i)
    {
      newTexCoords[i].Set((points[i][0] + 1.0f) * 0.5f, (points[i][2] + 1.0f) * 0.5f);
    }
  }

  if (points.size() != normals.size() || points.size() != newTexCoords.size())
  {
    std::cerr << "points:" << points.size() << '\n';
    std::cerr << "normals:" << normals.size() << '\n';
    std::cerr << "texCoords:" << newTexCoords.size() << '\n';
    std::cerr << "Error: points, normals, texCoords size mismatch" << '\n';
    return;
  }

  for (size_t i = 0; i < points.size(); ++i)
  {
    VertexObj vertex;
    vertex.pos = glm::vec3(points[i][0], points[i][1], points[i][2]);
    vertex.nrm = glm::vec3(normals[i][0], normals[i][1], normals[i][2]);
    vertex.texCoord = glm::vec2(newTexCoords[i][0], 1 - newTexCoords[i][1]);
    if (tangents.size() == points.size() && bitangentSigns.size() == points.size())
    {
      vertex.tangent = glm::vec4(tangents[i][0], tangents[i][1], tangents[i][2], bitangentSigns[i]);
    }
    vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
    loader.m_vertices.push_back(vertex);
  }

  for (const auto &face : faces)
  {
    loader.m_indices.push_back(static_cast<uint32_t>(face[0]) + vertexOffset);
    loader.m_indices.push_back(static_cast<uint32_t>(face[1]) + vertexOffset);
    loader.m_indices.push_back(static_cast<uint32_t>(face[2]) + vertexOffset);
  }

  for (const auto &matId : matIdx)
  {
    loader.m_matIndx.push_back(static_cast<uint32_t>(matId));
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
