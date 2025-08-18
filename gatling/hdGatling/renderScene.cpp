#include "renderScene.h"
#include <iostream>
#include <iomanip>

PXR_NAMESPACE_OPEN_SCOPE

// void GeneratePlanarUV(const VtVec3fArray& points, VtVec2fArray& texCoords, 
//                      const GfVec3f& planeNormal = GfVec3f(0,1,0)) {
//     texCoords.resize(points.size());
//     GfVec3f uAxis, vAxis;
    
//     // 计算UV轴
//     if (fabs(planeNormal[0]) >= fabs(planeNormal[1]) && 
//         fabs(planeNormal[0]) >= fabs(planeNormal[2])) {
//         uAxis = GfVec3f(0,1,0).Cross(planeNormal);
//     } else {
//         uAxis = GfVec3f(1,0,0).Cross(planeNormal);
//     }
//     vAxis = planeNormal.Cross(uAxis);
    
//     uAxis.Normalize();
//     vAxis.Normalize();
    
//     // 计算边界
//     GfRange2f bounds;
//     for (const auto& p : points) {
//         bounds.UnionWith(GfVec2f(p.Dot(uAxis), p.Dot(vAxis)));
//     }
    
//     // 生成UV
//     for (size_t i = 0; i < points.size(); ++i) {
//         GfVec2f proj(points[i].Dot(uAxis), points[i].Dot(vAxis));
//         texCoords[i].Set(
//             (proj[0] - bounds.GetMin()[0]) / bounds.GetSize()[0],
//             (proj[1] - bounds.GetMin()[1]) / bounds.GetSize()[1]
//         );
//     }
// }

// 转换函数
// 辅助函数：将 v_mesh 转换为 UsdLoader
void ConvertVmeshToLoader(const _VertexStreams& mesh, UsdLoader& Loader) {
    size_t vertexOffset = 0;

    Loader.m_vertices.clear();
    Loader.m_indices.clear();

    const auto& points = mesh.points;
    const auto& normals = mesh.normals;
    const auto& texCoords = mesh.texCoords;
    const auto& faces = mesh.faces;
    const auto& mat_idx = mesh.materialIds;

    VtVec2fArray new_texCoords = texCoords;
    // 在加载或处理网格数据的地方添加以下代码
    if (new_texCoords.empty() || new_texCoords.size() != points.size()) {
        new_texCoords.resize(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            // 简单映射：使用顶点x和z坐标归一化后的值
            new_texCoords[i].Set(
                (points[i][0] + 1.0f) * 0.5f,  // 将[-1,1]映射到[0,1]
                (points[i][2] + 1.0f) * 0.5f);
        }
    }

    if (points.size() != normals.size() || points.size() != new_texCoords.size()) {
        std::cerr << "points:" << points.size() << std::endl; 
        std::cerr << "normals:" << normals.size() << std::endl;
        std::cerr << "texCoords:" << new_texCoords.size() << std::endl;
        std::cerr << "错误：points, normals, texCoords 的长度不一致" << std::endl;
        return;
    }

    for (size_t i = 0; i < points.size(); ++i) {
        VertexObj vertex;
        vertex.pos = glm::vec3(points[i][0], points[i][1], points[i][2]);
        vertex.nrm = glm::vec3(normals[i][0], normals[i][1], normals[i][2]);
        vertex.texCoord = glm::vec2(new_texCoords[i][0], 1 - new_texCoords[i][1]);
        vertex.color = glm::vec3(1.0f, 1.0f, 1.0f); // 默认颜色
        Loader.m_vertices.push_back(vertex);
    }

    for (const auto& face : faces) {
        Loader.m_indices.push_back(static_cast<uint32_t>(face[0]) + vertexOffset);
        Loader.m_indices.push_back(static_cast<uint32_t>(face[1]) + vertexOffset);
        Loader.m_indices.push_back(static_cast<uint32_t>(face[2]) + vertexOffset);
    }

    for (const auto& mat_id : mat_idx) {
        Loader.m_matIndx.push_back(static_cast<uint32_t>(mat_id));
    }


    vertexOffset += points.size();
}

void add_default_material(UsdLoader& Loader)
{
    // 2. 创建默认材质
    MaterialObj mat;
    mat.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    mat.diffuse = glm::vec3(0.7f, 0.7f, 0.7f);
    mat.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    mat.shininess = 50.0f;
    mat.dissolve = 1.0f;
    mat.ior = 1.5f;
    mat.textureID = 0;
    mat.illum = 2; // 默认使用Phong光照模型

    // 如果没有材质，添加默认材质
    if (Loader.m_materials.empty()) {
        Loader.m_materials.emplace_back(mat);
    }

    //初始化材质索引
    // Loader.m_matIndx = std::vector<int32_t>(Loader.m_vertices.size()/3, 0);
    // for(int i = 0; i < Loader.m_matIndx.size(); i++){
    //     if(i*2 > Loader.m_matIndx.size()){
    //         Loader.m_matIndx[i] = 0;
    //     } else {
    //         Loader.m_matIndx[i] = 1;
    //     }
    // }

    //测试代码
    mat.textureID = 1;
    Loader.m_materials.emplace_back(mat);

    mat.textureID = 2;
    Loader.m_materials.emplace_back(mat);
}

// 函数：打印 v_mesh 和 loader 的信息以验证转换结果
void PrintLoader(const UsdLoader& loader, int n) {
    // 设置浮点数输出格式：固定小数点，精度为 6 位
    std::cout << std::fixed << std::setprecision(6);

    // // 打印 m_vertices 信息
    std::cout << "m_vertices 数量: " << loader.m_vertices.size() << std::endl;
    std::cout << "前 " << std::min(n, static_cast<int>(loader.m_vertices.size())) << " 个顶点:" << std::endl;
    int start = 0;
    for (int i = start; i < start+ n && i < loader.m_vertices.size(); ++i) {
        const auto& v = loader.m_vertices[i];
        std::cout << "  顶点[" << i << "]:" << std::endl;
        std::cout << "    pos: (" << v.pos.x << ", " << v.pos.y << ", " << v.pos.z << ")" << std::endl;
        std::cout << "    nrm: (" << v.nrm.x << ", " << v.nrm.y << ", " << v.nrm.z << ")" << std::endl;
        std::cout << "    color: (" << v.color.x << ", " << v.color.y << ", " << v.color.z << ")" << std::endl;
        std::cout << "    texCoord: (" << v.texCoord.x << ", " << v.texCoord.y << ")" << std::endl;
    }

    // // 打印 m_indices 信息
    // std::cout << "m_indices 数量: " << loader.m_indices.size() << std::endl;
    // std::cout << "前 " << std::min(n, static_cast<int>(loader.m_indices.size())) << " 个索引:" << std::endl;
    // for (int i = 0; i < n && i < loader.m_indices.size(); ++i) {
    //     std::cout << "  索引[" << i << "]: " << loader.m_indices[i] << std::endl;
    // }

    // // 打印 m_materials 信息
    std::cout << "m_materials 数量: " << loader.m_materials.size() << std::endl;
    std::cout << "前 " << std::min(n, static_cast<int>(loader.m_materials.size())) << " 个材质:" << std::endl;
    for (int i = 0; i < n && i < loader.m_materials.size(); ++i) {
        const auto& m = loader.m_materials[i];
        std::cout << "  材质[" << i << "]:" << std::endl;
        std::cout << "    ambient: (" << m.ambient.x << ", " << m.ambient.y << ", " << m.ambient.z << ")" << std::endl;
        std::cout << "    diffuse: (" << m.diffuse.x << ", " << m.diffuse.y << ", " << m.diffuse.z << ")" << std::endl;
        std::cout << "    specular: (" << m.specular.x << ", " << m.specular.y << ", " << m.specular.z << ")" << std::endl;
        std::cout << "    transmittance: (" << m.transmittance.x << ", " << m.transmittance.y << ", " << m.transmittance.z << ")" << std::endl;
        std::cout << "    emission: (" << m.emission.x << ", " << m.emission.y << ", " << m.emission.z << ")" << std::endl;
        std::cout << "    shininess: " << m.shininess << std::endl;
        std::cout << "    ior: " << m.ior << std::endl;
        std::cout << "    dissolve: " << m.dissolve << std::endl;
        std::cout << "    illum: " << m.illum << std::endl;
        std::cout << "    textureID: " << m.textureID << std::endl;
    }

    // // 打印 m_textures 信息
    std::cout << "m_textures 数量: " << loader.m_textures.size() << std::endl;
    std::cout << "前 " << std::min(n, static_cast<int>(loader.m_textures.size())) << " 个纹理:" << std::endl;
    for (int i = 0; i < n && i < loader.m_textures.size(); ++i) {
        std::cout  << loader.m_textures[i] << ", ";
    }
    // std::cout << std::endl;

    // // 打印 m_matIndx 信息
    std::cout << "m_matIndx 数量: " << loader.m_matIndx.size() << std::endl;
    std::cout << "前 " << std::min(n, static_cast<int>(loader.m_matIndx.size())) << " 个材质索引:" << std::endl;
    for (int i = 0; i < n && i < loader.m_matIndx.size(); ++i) {
        std::cout  << loader.m_matIndx[i] << ", ";
    }
    std::cout << std::endl;
}

// 主函数：比较 v_mesh 转换后的 loader 与输入的 loader
void compareLoaders(const UsdLoader& tempLoader, const UsdLoader& loader) {
    // 步骤 2：比较 tempLoader 和 loader
    // 比较 m_vertices
    if (tempLoader.m_vertices.size() != loader.m_vertices.size()) {
        std::cout << "m_vertices 数量不一致: tempLoader 有 " << tempLoader.m_vertices.size()
                  << ", loader 有 " << loader.m_vertices.size() << std::endl;
    }

    bool verticesMatch = true;
    for (size_t i = 0; i < 5; ++i) {
        const auto& v1 = tempLoader.m_vertices[i];
        const auto& v2 = loader.m_vertices[i];
        if (v1.pos != v2.pos || v1.nrm != v2.nrm || v1.texCoord != v2.texCoord || v1.color != v2.color) {
            std::cout << "m_vertices[" << i << "] 不一致:" << std::endl;
            std::cout << "  ConvLoader: pos(" << v1.pos.x << ", " << v1.pos.y << ", " << v1.pos.z << "), "
                      << "nrm(" << ", " << v1.nrm.y << ", " << v1.nrm.z << "), "
                      << "texCoord(" << v1.texCoord.x << ", " << v1.texCoord.y << "), "
                      << "color(" << v1.color.x << ", " << v1.color.y << ", " << v1.color.z << ")" << std::endl;
            std::cout << "  RealLoader: pos(" << v2.pos.x << ", " << v2.pos.y << ", " << v2.pos.z << "), "
                      << "nrm(" << v2.nrm.x << ", " << v2.nrm.y << ", " << v2.nrm.z << "), "
                      << "texCoord(" << v2.texCoord.x << ", " << v2.texCoord.y << "), "
                      << "color(" << v2.color.x << ", " << v2.color.y << ", " << v2.color.z << ")" << std::endl;
            verticesMatch = false;
        }
    }

    // 比较 m_indices
    if (tempLoader.m_indices.size() != loader.m_indices.size()) {
        std::cout << "m_indices 数量不一致: tempLoader 有 " << tempLoader.m_indices.size()
                  << ", loader 有 " << loader.m_indices.size() << std::endl;
    }

    bool indicesMatch = true;
    for (size_t i = 0; i < 5; ++i) {
        if (tempLoader.m_indices[i] != loader.m_indices[i]) {
            std::cout << "m_indices[" << i << "] 不一致: tempLoader = " << tempLoader.m_indices[i]
                      << ", loader = " << loader.m_indices[i] << std::endl;
            indicesMatch = false;
        }
    }

    // 步骤 3：输出比较结果
    if (verticesMatch && indicesMatch) {
        std::cout << "数据完全一致" << std::endl;
    } else {
        std::cout << "数据不一致" << std::endl;
    }
}
PXR_NAMESPACE_CLOSE_SCOPE