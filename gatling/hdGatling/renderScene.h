#pragma once
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/gf/matrix4f.h>

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
#include <ModelLoader.h>
#include <glm/glm.hpp>

PXR_NAMESPACE_OPEN_SCOPE

struct _VertexStreams
{
  VtVec3iArray faces;
  VtVec3fArray points;
  VtVec3fArray normals;
  VtVec2fArray texCoords;
  VtIntArray   materialIds;      // 新增：每个面的材质 ID
  MaterialObj  materialObj;      //对应的材质
  bool         _visible = true;  // 当前mesh是否可见

  bool                   _blas_changed     = false;  //blas是否被更新，基础顶点和mesh结构
  bool                   _tlas_changed     = false;  //tlas是否被更新，变换矩阵
  bool                   _material_changed = false;
  int                    _mesh_id;  //在cpu和gpu的vector中的序号，必须保持一致
  std::vector<glm::mat4> _instanceTransforms = {glm::mat4{1}};
  glm::mat4              _transform          = glm::mat4{1};
  std::vector<glm::mat4> _tlas_trans;
};

struct HdGatlingScene
{
  //互斥锁，防止多线程mesh抢夺资源崩溃
  std::mutex mutex;
  // 转化成raytrace可用的mesh格式
  std::vector<_VertexStreams> v_mesh;
};

// Matrix conversion utility function
// Converts GfMatrix4f (row-major) to glm::mat4 (column-major)
static inline glm::mat4 GfToGlm(const GfMatrix4f& m) {
    glm::mat4 result(1.0f);
    for(int row = 0; row < 4; ++row) {
        for(int col = 0; col < 4; ++col) {
            result[col][row] = m[row][col]; // Note: glm[col][row] = gf[row][col]
        }
    }
    return result;
}


//添加默认材质
void add_default_material(ModelLoader& Loader);
void ConvertVmeshToLoader(const _VertexStreams& v_mesh, ModelLoader& Loader);
void compareLoaders(const ModelLoader& tempLoader, const ModelLoader& loader);

// Debug functions for matrix verification
void printMatrix(const glm::mat4& m, const std::string& name);
void printGfMatrix(const GfMatrix4f& m, const std::string& name);

//根据模型大小生成一个自定义的scale矩阵
void PrintLoader(const ModelLoader& loader, int n = 5);
PXR_NAMESPACE_CLOSE_SCOPE