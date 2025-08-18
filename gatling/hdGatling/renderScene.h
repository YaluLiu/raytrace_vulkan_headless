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
#include <ModelLoader.h>

PXR_NAMESPACE_OPEN_SCOPE

struct _VertexStreams
{
    VtVec3iArray faces;
    VtVec3fArray points;
    VtVec3fArray normals;
    VtVec2fArray texCoords;
    VtIntArray materialIds;      // 新增：每个面的材质 ID

    bool         _changed;      //是否被更新
    int          _mesh_id;      //在cpu和gpu的vector中的序号，必须保持一致
    glm::mat4    _transform;
};

struct HdGatlingScene
{
    //互斥锁，防止多线程mesh抢夺资源崩溃
    std::mutex mutex;
    // 转化成raytrace可用的mesh格式
    std::vector<_VertexStreams> v_mesh;
};



//添加默认材质
void add_default_material(ModelLoader& Loader);
void ConvertVmeshToLoader(const _VertexStreams& v_mesh, ModelLoader& Loader);
void compareLoaders(const ModelLoader& tempLoader, const ModelLoader& loader);

//根据模型大小生成一个自定义的scale矩阵
void PrintLoader(const ModelLoader& loader, int n=5); 
PXR_NAMESPACE_CLOSE_SCOPE