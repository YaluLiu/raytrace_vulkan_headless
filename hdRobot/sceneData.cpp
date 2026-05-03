#include "sceneData.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

void HydraMaterial::set_default()
{
  diffuse = glm::vec3(0.18f, 0.18f, 0.18f);
  baseColorFactor = diffuse;
  emission = glm::vec3(0.0f, 0.0f, 0.0f);
  emissionFactor = emission;
  metallicFactor = 0.0f;
  roughnessFactor = 0.5f;
  opacityFactor = 1.0f;
  textureID = -1;
  baseColorTextureId = -1;
  metallicTextureId = -1;
  roughnessTextureId = -1;
  normalTextureId = -1;
  emissionTextureId = -1;
  opacityTextureId = -1;
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
  mat.shininess = shininess;
  mat.ior = ior;
  mat.dissolve = dissolve;
  mat.metallicFactor = metallicFactor;
  mat.roughnessFactor = roughnessFactor;
  mat.opacityFactor = opacityFactor;
  mat.illum = illum;
  mat.textureID = textureID;
  mat.baseColorTextureId = baseColorTextureId;
  mat.metallicTextureId = metallicTextureId;
  mat.roughnessTextureId = roughnessTextureId;
  mat.normalTextureId = normalTextureId;
  mat.emissionTextureId = emissionTextureId;
  mat.opacityTextureId = opacityTextureId;
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

int TextureRegistry::Register(const std::string &texturePath)
{
  const auto it = _textureIdByPath.find(texturePath);
  if (it != _textureIdByPath.end())
  {
    return it->second;
  }

  const int textureId = static_cast<int>(_texturePaths.size());
  _texturePaths.emplace_back(texturePath);
  _textureIdByPath.emplace(texturePath, textureId);
  return textureId;
}

const std::vector<std::string> &TextureRegistry::GetPaths() const
{
  return _texturePaths;
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
