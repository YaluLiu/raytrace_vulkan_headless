#pragma once
#include "ModelLoader.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/materialBindingAPI.h> // 添加缺失的头文件
#include <vector>
#include <string>




class UsdLoader : public ModelLoader {
public:
    void loadModel(const std::string& filename);

    //自定义的的分开接口
    void loadVertices(pxr::UsdGeomMesh& mesh);
    void loadNormals(pxr::UsdGeomMesh& mesh);
    void loadTexCoords(pxr::UsdGeomMesh& mesh);
    void loadIndices(pxr::UsdGeomMesh& mesh);
    void loadMaterial(const pxr::UsdPrim& prim);

    //计算法线，当d没有的时候
    void computeVertexNormals();
    void assignMaterialIndices(const pxr::UsdPrim& prim, size_t faceCount, int materialIndex);

    // 材质路径到索引的映射（避免重复加载）
    std::map<pxr::SdfPath, int> m_materialIndexMap;
};