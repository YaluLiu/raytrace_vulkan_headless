#pragma once

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/imaging/hd/tokens.h>

#include <glm/glm.hpp>
#include <engine/texture_asset.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include <engine/shaders/host_device.h>

PXR_NAMESPACE_OPEN_SCOPE

struct HydraMesh
{
  VtVec3iArray faces;
  VtVec3fArray points;
  VtVec3fArray normals;
  VtVec2fArray texCoords;
  VtVec3fArray tangents;
  VtFloatArray bitangentSigns;
  VtIntArray materialIds;
  bool visible = true;
  bool valid = true;
  TfToken renderTag = HdRenderTagTokens->geometry;
  TfToken traceRole;

  bool geometry_changed = false;
  bool instance_changed = false;

  std::vector<int> scene_mat_ids;
  std::vector<glm::mat4> instanceTransforms = {glm::mat4{1.0f}};
  bool hasInstances = true;
  glm::mat4 transform = glm::mat4{1.0f};
};

struct HydraMaterial
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
  float shininess = 0.0f;
  float ior = 1.0f;
  float opaque = 1.0f;
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
  std::string texturePath;
  bool material_changed = false;

  void set_default();
};

struct HydraLight
{
  int type;
  int valid = 0;
  vec3 baseEmission;
  float diffuse;
  float specular;

  vec3 direction;
  float angle;

  vec3 position;
  float radius;

  vec4 rotateQuat;
  int textureID = -1;
  std::string texturePath;
};

struct TextureRegistry
{
  int Register(const std::string &texturePath, TextureUsage usage = TextureUsage::BaseColor);
  const std::vector<std::string> &GetPaths() const;
  const std::vector<TextureAsset> &GetTextureAssets() const;
  uint64_t GetVersion() const;

 private:
  std::vector<std::string> _texturePaths;
  std::vector<TextureAsset> _textureAssets;
  std::unordered_map<std::string, int> _textureIdByPath;
  uint64_t _version = 0;
};

TextureColorSpace TextureColorSpaceForUsage(TextureUsage usage);

PXR_NAMESPACE_CLOSE_SCOPE
