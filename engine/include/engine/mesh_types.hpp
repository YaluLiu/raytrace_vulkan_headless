#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

struct Material
{
  glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
  glm::vec3 diffuse = glm::vec3(0.18f, 0.18f, 0.18f);
  glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 emission = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 baseColorFactor = glm::vec3(0.18f, 0.18f, 0.18f);
  glm::vec3 emissionFactor = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 transmissionColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::vec3 subsurfaceColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  float shininess = 0.f;
  float ior = 1.0f;
  float opaque = 1.f;
  float metallicFactor = 0.0f;
  float roughnessFactor = 0.5f;
  float opacityFactor = 1.0f;
  float transmissionFactor = 0.0f;
  float subsurfaceFactor = 0.0f;
  float subsurfaceScale = 0.0f;
  int illum = 0;
  int diffuseTextureId = -1;
  int baseColorTextureId = -1;
  int metallicTextureId = -1;
  int roughnessTextureId = -1;
  int normalTextureId = -1;
  int emissionTextureId = -1;
  int opacityTextureId = -1;
  int subsurfaceTextureId = -1;
};

struct MeshVertex
{
  glm::vec3 pos;
  glm::vec3 nrm;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec4 tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
};

struct MeshGeometry
{
  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<int32_t> materialIndices;
};
