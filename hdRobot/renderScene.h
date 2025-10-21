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

struct _VertexStreams
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
  int                    material_id        = -1;  //对应的材质在scene队列的id
  std::vector<glm::mat4> instanceTransforms = {glm::mat4{1}};
  glm::mat4              transform          = glm::mat4{1};
  std::vector<int>       tlasIds;  // 当前mesh的实例在tlas队列中对应的id[0-n]
};

struct HdGatlingScene
{
  //multi thread mutex
  std::mutex mutex;
  // 转化成raytrace可用的mesh格式
  std::vector<_VertexStreams> v_mesh;
  std::vector<MaterialObj>    v_mat;  //对应的材质
  std::vector<Light>          v_light;
  std::vector<Sphere>         v_sphere;

  // material count
  int mat_count = 0;
};


//添加默认材质
void add_default_material(ModelLoader& Loader);
void ConvertVmeshToLoader(const _VertexStreams& v_mesh, ModelLoader& Loader);
void compareLoaders(const ModelLoader& tempLoader, const ModelLoader& loader);

//根据模型大小生成一个自定义的scale矩阵
void PrintLoader(const ModelLoader& loader, int n = 5);
PXR_NAMESPACE_CLOSE_SCOPE