#pragma once
#include <glm/glm.hpp>
#include <string>

// 定义MaterialObj结构体，表示材质信息
struct MaterialObj
{
  glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
  glm::vec3 diffuse = glm::vec3(0.18f, 0.18f, 0.18f);
  glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 emission = glm::vec3(0.0f, 0.0f, 0.10);
  glm::vec3 baseColorFactor = glm::vec3(0.18f, 0.18f, 0.18f);
  glm::vec3 emissionFactor = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 transmissionColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::vec3 subsurfaceColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  float shininess = 0.f;
  float ior = 1.0f;      // index of refraction
  float dissolve = 1.f;  // 1 == opaque; 0 == fully transparent
                         // 光照模型（参见MTL文件格式说明）
  float metallicFactor = 0.0f;
  float roughnessFactor = 0.5f;
  float opacityFactor = 1.0f;
  float transmissionFactor = 0.0f;
  float subsurfaceFactor = 0.0f;
  float subsurfaceScale = 0.0f;
  int illum = 0;
  int textureID = -1;
  int baseColorTextureId = -1;
  int metallicTextureId = -1;
  int roughnessTextureId = -1;
  int normalTextureId = -1;
  int emissionTextureId = -1;
  int opacityTextureId = -1;
  int subsurfaceTextureId = -1;
};

// OBJ模型的顶点结构体
// 注意：BLAS构建器依赖于pos为第一个成员
struct VertexObj
{
  // 顶点坐标
  glm::vec3 pos;
  // 顶点法线
  glm::vec3 nrm;
  // 顶点颜色
  glm::vec3 color;
  // 顶点纹理坐标
  glm::vec2 texCoord;
  glm::vec4 tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
};

// 形状结构体，保存每个子mesh的索引信息
struct shapeObj
{
  // 顶点偏移
  uint32_t offset;
  // 索引数量
  uint32_t nbIndex;
  // 材质索引
  uint32_t matIndex;
};
