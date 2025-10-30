#pragma once
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <atomic>
#include <optional>
#include <mutex>
#include <assert.h>
#include <glm/glm.hpp>
#include <ModelLoader.h>
#include "shaders/host_device.h"

PXR_NAMESPACE_OPEN_SCOPE

struct HydraMesh
{
  VtVec3iArray faces;
  VtVec3fArray points;
  VtVec3fArray normals;
  VtVec2fArray texCoords;
  VtIntArray   materialIds;     // material ids, in gpu buffer, [0*100,1*100], as two boot
  bool         visible = true;  // if mesh is visible
  bool         valid   = true;

  bool blas_changed = false;
  bool tlas_changed = false;
  //--------------------------------------------------------
  std::vector<int>       scene_mat_ids;  //对应的材质在scene队列的id
  std::vector<glm::mat4> instanceTransforms = {glm::mat4{1}};
  glm::mat4              transform          = glm::mat4{1};
  std::vector<int>       tlasIds;  // 当前mesh的实例在tlas队列中对应的id[0-n]
};

struct HydraMaterial
{
  // 环境光成分
  glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
  // 漫反射成分
  glm::vec3 diffuse = glm::vec3(0.18f, 0.18f, 0.18f);
  // 镜面反射成分
  glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f);
  // 透射成分
  glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
  // 自发光成分
  glm::vec3 emission = glm::vec3(0.0f, 0.0f, 0.10);
  // 高光系数
  float shininess = 0.f;
  // 折射率
  float ior = 1.0f;  // index of refraction
  // 透明度（1为不透明，0为完全透明）
  float dissolve = 1.f;  // 1 == opaque; 0 == fully transparent
                         // 光照模型（参见MTL文件格式说明）
  int illum = 0;
  // 贴图ID（如果没有贴图则为-1）
  int         textureID = -1;
  std::string texturePath;
  bool        material_changed = false;
  void        set_default()
  {
    diffuse          = glm::vec3(0.18f, 0.18f, 0.18f);
    material_changed = true;
  }
  MaterialObj toMaterialObj() const
  {
    MaterialObj mat;
    mat.ambient       = this->ambient;
    mat.diffuse       = this->diffuse;
    mat.specular      = this->specular;
    mat.transmittance = this->transmittance;
    mat.emission      = this->emission;
    mat.shininess     = this->shininess;
    mat.ior           = this->ior;
    mat.dissolve      = this->dissolve;
    mat.illum         = this->illum;
    mat.textureID     = this->textureID;
    return mat;
  }
};

struct HydraLight
{
  // common
  int   type;
  int   valid = 1;
  vec3  baseEmission;  // intensity * color * colorTemp * exposure
  float diffuseScale;
  float specularScale;
  // distant light
  vec3  direction;
  float angleScale;
  // sphere light
  vec3  position;
  float radius;
  // dome light
  vec4        rotateQuat;
  int         textureID = -1;
  std::string texturePath;
  Light       toLight() const
  {
    Light light;
    light.type          = type;
    light.baseEmission  = baseEmission;
    light.diffuseScale  = diffuseScale;
    light.specularScale = specularScale;
    light.direction     = direction;
    light.angleScale    = angleScale;
    light.position      = position;
    light.radius        = radius;
    light.rotateQuat    = rotateQuat;
    light.textureID     = textureID;
    return light;
  }
};

struct HdGatlingScene
{
  //multi thread mutex
  std::mutex mutex;
  // 转化成raytrace可用的mesh格式
  std::vector<HydraMesh>     v_mesh;
  std::vector<HydraMaterial> v_mat;
  std::vector<HydraLight>    v_light;
  std::vector<Sphere>        v_sphere;
  std::vector<std::string>   v_texturePath;  //material&domelight, textures
};


//添加默认材质
void add_default_material(ModelLoader& Loader);
void ConvertVmeshToLoader(const HydraMesh& v_mesh, ModelLoader& Loader);
void compareLoaders(const ModelLoader& tempLoader, const ModelLoader& loader);

//根据模型大小生成一个自定义的scale矩阵
void PrintLoader(const ModelLoader& loader, int n = 5);
PXR_NAMESPACE_CLOSE_SCOPE