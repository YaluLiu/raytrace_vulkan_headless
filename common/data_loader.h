#pragma once
#include <glm/glm.hpp>

// 定义MaterialObj结构体，表示材质信息
struct MaterialObj
{
  // 环境光成分
  glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
  // 漫反射成分
  glm::vec3 diffuse = glm::vec3(0.7f, 0.7f, 0.7f);
  // 镜面反射成分
  glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
  // 透射成分
  glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
  // 自发光成分
  glm::vec3 emission = glm::vec3(0.0f, 0.0f, 0.10);
  // 高光系数
  float shininess = 0.f;
  // 折射率
  float ior = 1.0f; // index of refraction
  // 透明度（1为不透明，0为完全透明）
  float dissolve = 1.f; // 1 == opaque; 0 == fully transparent
                        // 光照模型（参见MTL文件格式说明）
  int illum = 0;
  // 贴图ID（如果没有贴图则为-1）
  int textureID = -1;
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
