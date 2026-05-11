#include "usd_loader.h"
#include <algorithm>
#include <iostream>
#include <format>

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
MaterialObj MakeDefaultUsdMaterial()
{
  MaterialObj mat;
  mat.ambient         = glm::vec3(0.1f, 0.1f, 0.1f);
  mat.diffuse         = glm::vec3(0.7f, 0.7f, 0.7f);
  mat.baseColorFactor = mat.diffuse;
  mat.specular        = glm::vec3(0.5f, 0.5f, 0.5f);
  mat.shininess       = 50.0f;
  mat.opaque          = 1.0f;
  mat.opacityFactor   = mat.opaque;
  mat.ior             = 1.5f;
  mat.illum           = 2;
  return mat;
}

bool ReadInputVec3(const UsdShadeInput& input, glm::vec3* result)
{
  if(!input || result == nullptr)
  {
    return false;
  }

  GfVec3f color;
  if(input.Get(&color))
  {
    *result = glm::vec3(color[0], color[1], color[2]);
    return true;
  }
  return false;
}

bool ReadInputFloat(const UsdShadeInput& input, float* result)
{
  if(!input || result == nullptr)
  {
    return false;
  }
  return input.Get(result);
}

std::string GetAssetPathString(const SdfAssetPath& assetPath)
{
  std::string path = assetPath.GetResolvedPath();
  if(path.empty())
  {
    path = assetPath.GetAssetPath();
  }
  return path;
}

std::string ReadUsdTextureFile(const UsdShadeShader& shader)
{
  if(!shader)
  {
    return {};
  }

  TfToken id;
  shader.GetIdAttr().Get(&id);
  if(id != TfToken("UsdUVTexture"))
  {
    return {};
  }

  const UsdShadeInput fileInput = shader.GetInput(TfToken("file"));
  if(!fileInput)
  {
    return {};
  }

  SdfAssetPath textureAsset;
  if(!fileInput.Get(&textureAsset))
  {
    return {};
  }
  return GetAssetPathString(textureAsset);
}

TfToken ReadPrimvarReaderName(const UsdShadeShader& shader)
{
  if(!shader)
  {
    return TfToken();
  }

  TfToken id;
  shader.GetIdAttr().Get(&id);
  const std::string shaderId = id.GetString();
  if(shaderId.find("UsdPrimvarReader") == std::string::npos)
  {
    return TfToken();
  }

  const UsdShadeInput varnameInput = shader.GetInput(TfToken("varname"));
  if(!varnameInput)
  {
    return TfToken();
  }

  std::string varname;
  if(varnameInput.Get(&varname) && !varname.empty())
  {
    return TfToken(varname);
  }

  TfToken varnameToken;
  if(varnameInput.Get(&varnameToken))
  {
    return varnameToken;
  }
  return TfToken();
}

bool GetConnectedShader(const UsdShadeInput& input, UsdShadeShader* shader)
{
  if(!input || shader == nullptr)
  {
    return false;
  }

  UsdShadeConnectableAPI source;
  TfToken                outputName;
  UsdShadeAttributeType  sourceType;
  if(!input.GetConnectedSource(&source, &outputName, &sourceType))
  {
    return false;
  }

  *shader = UsdShadeShader(source);
  return static_cast<bool>(*shader);
}
}  // namespace

void printRedError(const std::string& message)
{
  std::cerr << "\033[31m" << "[Error] " << message << "\033[0m" << std::endl;
}

void UsdLoader::loadVertices(UsdGeomMesh& mesh)
{
  VtArray<GfVec3f> points;
  mesh.GetPointsAttr().Get(&points);
  std::vector<VertexObj> origin_vertices;
  origin_vertices.reserve(points.size());
  for(const auto& point : points)
  {
    VertexObj vertex;
    vertex.pos   = glm::vec3(point[0], point[1], point[2]);
    vertex.color = glm::vec3(1.0f);  // 默认颜色
    origin_vertices.push_back(vertex);
  }
  m_vertices.reserve(m_indices.size());
  for(const auto& indice_idx : m_indices)
  {
    VertexObj vertex;
    vertex.pos   = origin_vertices[indice_idx].pos;
    vertex.color = origin_vertices[indice_idx].color;
    m_vertices.push_back(vertex);
  }
  size_t size_indices = m_indices.size();
  m_indices.clear();
  for(size_t i = 0; i < size_indices; ++i)
  {
    m_indices.push_back(i);
  }
}

void UsdLoader::loadNormals(pxr::UsdGeomMesh& mesh)
{
  pxr::VtArray<pxr::GfVec3f> normals;
  pxr::VtArray<int>          normalsIndices;  // 用于存储可能的法线索引

  // 首先尝试默认 normals 属性
  if(mesh.GetNormalsAttr().IsDefined())
  {
    mesh.GetNormalsAttr().Get(&normals);
    if(!normals.empty())
    {
      // 直接赋值给顶点
      for(size_t i = 0; i < m_vertices.size() && i < normals.size(); ++i)
      {
        m_vertices[i].nrm = glm::vec3(normals[i][0], normals[i][1], normals[i][2]);
      }
      return;
    }
  }

  // 如果默认 normals 未定义，尝试 primvars:normals
  UsdGeomPrimvarsAPI primvarsAPI(mesh);
  UsdGeomPrimvar     normalsPrimvar = primvarsAPI.GetPrimvar(TfToken("normals"));
  if(normalsPrimvar && normalsPrimvar.IsDefined())
  {
    if(normalsPrimvar.Get(&normals))
    {
      // 检查是否有 normalsIndices
      UsdGeomPrimvar indicesPrimvar = primvarsAPI.GetPrimvar(TfToken("normalsIndices"));
      if(indicesPrimvar && indicesPrimvar.Get(&normalsIndices))
      {
        // 使用索引映射法线到顶点
        for(size_t i = 0; i < m_vertices.size() && i < normalsIndices.size(); ++i)
        {
          int idx = normalsIndices[i];
          if(idx >= 0 && static_cast<size_t>(idx) < normals.size())
          {
            m_vertices[i].nrm = glm::vec3(normals[idx][0], normals[idx][1], normals[idx][2]);
          }
        }
      }
      else
      {
        // 没有索引，直接按顺序赋值
        for(size_t i = 0; i < m_vertices.size() && i < normals.size(); ++i)
        {
          m_vertices[i].nrm = glm::vec3(normals[i][0], normals[i][1], normals[i][2]);
        }
      }
      return;
    }
  }

  // 如果仍未获取到法线，计算法线
  printRedError("No normals (normals or primvars:normals) found, computing vertex normals.\n");
  computeVertexNormals();
}

void UsdLoader::computeVertexNormals()
{
  std::unordered_map<size_t, glm::vec3> normalAccum;

  // 遍历每个面（假设是三角形）
  for(size_t i = 0; i < m_indices.size(); i += 3)
  {
    if(i + 2 >= m_indices.size())
      break;  // 防止越界
    uint32_t idx0 = m_indices[i];
    uint32_t idx1 = m_indices[i + 1];
    uint32_t idx2 = m_indices[i + 2];

    if(idx0 >= m_vertices.size() || idx1 >= m_vertices.size() || idx2 >= m_vertices.size())
    {
      continue;  // 跳过无效索引
    }

    glm::vec3 v0 = m_vertices[idx0].pos;
    glm::vec3 v1 = m_vertices[idx1].pos;
    glm::vec3 v2 = m_vertices[idx2].pos;

    glm::vec3 edge1      = v1 - v0;
    glm::vec3 edge2      = v2 - v0;
    glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

    normalAccum[idx0] += faceNormal;
    normalAccum[idx1] += faceNormal;
    normalAccum[idx2] += faceNormal;
  }

  // 平均并归一化
  for(auto& pair : normalAccum)
  {
    size_t idx = pair.first;
    if(idx < m_vertices.size())
    {
      m_vertices[idx].nrm = glm::normalize(pair.second);
    }
  }
}

void UsdLoader::loadTexCoords(UsdGeomMesh& mesh)
{
  UsdGeomPrimvarsAPI primvarsAPI(mesh);

  // 尝试常见的纹理坐标名称
  UsdGeomPrimvar stPrimvar = primvarsAPI.GetPrimvar(TfToken("st"));
  if(!stPrimvar)
    stPrimvar = primvarsAPI.GetPrimvar(TfToken("uv"));
  if(!stPrimvar)
    stPrimvar = primvarsAPI.GetPrimvar(TfToken("map1"));
  if(!stPrimvar)
    stPrimvar = primvarsAPI.GetPrimvar(TfToken("UVMap"));  // 添加 UVMap 检查

  if(!stPrimvar)
  {
    // 调试：列出所有可用的 primvars，检查可能的纹理坐标名称
    std::vector<UsdGeomPrimvar> primvars = primvarsAPI.GetPrimvars();
    std::cerr << "Available primvars: ";
    for(const auto& pv : primvars)
    {
      std::cerr << pv.GetPrimvarName() << " ";
    }
    std::cerr << std::endl;
    printRedError("No texture coordinates (primvars:st/uv/map1/UVMap) found.\n");
    return;
  }

  // 读取数据
  VtArray<GfVec2f> texCoords;
  if(!stPrimvar.Get(&texCoords))
  {
    std::cout << "[UsdLoader] Failed to read texture coordinates data.\n";
    return;
  }
  for(size_t i = 0; i < m_vertices.size(); ++i)
  {
    m_vertices[i].texCoord = glm::vec2(float(texCoords[i][0]), float(texCoords[i][1]));  // Flip V axis
  }
}

void UsdLoader::loadIndices(UsdGeomMesh& mesh)
{
  VtArray<int> faceVertexCounts, faceVertexIndices;
  mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
  mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
  int indexOffset = 0;
  for(int count : faceVertexCounts)
  {
    if(count == 3)
    {
      // 三角形，直接添加索引
      for(int i = 0; i < 3; ++i)
      {
        m_indices.push_back(faceVertexIndices[indexOffset + i]);
      }
    }
    else if(count == 4)
    {
      // 四边形拆分为两个三角形
      m_indices.push_back(faceVertexIndices[indexOffset]);
      m_indices.push_back(faceVertexIndices[indexOffset + 1]);
      m_indices.push_back(faceVertexIndices[indexOffset + 2]);
      m_indices.push_back(faceVertexIndices[indexOffset]);
      m_indices.push_back(faceVertexIndices[indexOffset + 2]);
      m_indices.push_back(faceVertexIndices[indexOffset + 3]);
    }
    indexOffset += count;
  }
}

void UsdLoader::loadMaterial(const pxr::UsdPrim& prim)
{
  UsdShadeShader shader;
  if(prim.IsA<UsdShadeMaterial>())
  {
    UsdShadeMaterial material(prim);
    shader = material.ComputeSurfaceSource();
  }
  else if(prim.IsA<UsdShadeShader>())
  {
    shader = UsdShadeShader(prim);
  }
  else
  {
    return;
  }

  MaterialObj mat = MakeDefaultUsdMaterial();

  TfToken idToken;
  shader.GetIdAttr().Get(&idToken);
  if(idToken != pxr::TfToken("UsdPreviewSurface"))
  {
    m_materials.push_back(mat);
    return;
  }

  UsdShadeInput diffuseInput = shader.GetInput(TfToken("diffuseColor"));
  if(diffuseInput)
  {
    glm::vec3 diffuseColor;
    if(ReadInputVec3(diffuseInput, &diffuseColor))
    {
      mat.diffuse = diffuseColor;
      mat.baseColorFactor = mat.diffuse;
      mat.ambient = mat.diffuse * 0.1f;
    }
  }

  UsdShadeInput specularInput = shader.GetInput(TfToken("specularColor"));
  if(!specularInput)
  {
    specularInput = shader.GetInput(TfToken("specular"));
  }
  if(specularInput)
  {
    if(specularInput.GetTypeName() == pxr::SdfValueTypeNames->Float)
    {
      float specularValue;
      if(specularInput.Get(&specularValue))
      {
        mat.specular = glm::vec3(specularValue);
      }
    }
    else if(specularInput.GetTypeName() == pxr::SdfValueTypeNames->Color3f)
    {
      pxr::GfVec3f specularColor;
      if(specularInput.Get(&specularColor))
      {
        mat.specular = glm::vec3(specularColor[0], specularColor[1], specularColor[2]);
      }
    }
  }

  UsdShadeInput metallicInput = shader.GetInput(TfToken("metallic"));
  ReadInputFloat(metallicInput, &mat.metallicFactor);

  UsdShadeInput roughnessInput = shader.GetInput(TfToken("roughness"));
  float roughness;
  if(ReadInputFloat(roughnessInput, &roughness))
  {
    mat.roughnessFactor = std::clamp(roughness, 0.0f, 1.0f);
    mat.shininess = 64.0f * (1.0f - std::clamp(roughness, 0.0f, 1.0f));
  }

  UsdShadeInput opacityInput = shader.GetInput(TfToken("opacity"));
  float opacity;
  if(ReadInputFloat(opacityInput, &opacity))
  {
    mat.opaque = std::clamp(opacity, 0.0f, 1.0f);
    mat.opacityFactor = mat.opaque;
  }

  UsdShadeInput iorInput = shader.GetInput(TfToken("ior"));
  ReadInputFloat(iorInput, &mat.ior);

  UsdShadeInput emissiveInput = shader.GetInput(TfToken("emissiveColor"));
  glm::vec3 emissionColor;
  if(ReadInputVec3(emissiveInput, &emissionColor))
  {
    mat.emission = emissionColor;
    mat.emissionFactor = emissionColor;
  }

  UsdShadeShader diffuseSource;
  if(GetConnectedShader(diffuseInput, &diffuseSource))
  {
    const std::string texturePath = ReadUsdTextureFile(diffuseSource);
    if(!texturePath.empty())
    {
      auto it = std::find(m_textures.begin(), m_textures.end(), texturePath);
      if(it == m_textures.end())
      {
        m_textures.push_back(texturePath);
        mat.diffuseTextureId = static_cast<int>(m_textures.size() - 1);
      }
      else
      {
        mat.diffuseTextureId = static_cast<int>(std::distance(m_textures.begin(), it));
      }
      mat.baseColorTextureId = mat.diffuseTextureId;
      mat.baseColorFactor = glm::vec3(1.0f);
      mat.diffuse = mat.baseColorFactor;
    }

    const TfToken primvarName = ReadPrimvarReaderName(diffuseSource);
    if(!primvarName.IsEmpty())
    {
      m_materialBaseColorPrimvars[prim.GetPath()] = primvarName;
    }
  }

  m_materials.push_back(mat);
}

bool UsdLoader::applyMaterialColorPrimvar(const pxr::UsdPrim& prim,
                                          const pxr::TfToken& primvarName,
                                          int sourceMaterialIndex,
                                          int* materialIndex)
{
  if(primvarName.IsEmpty() || materialIndex == nullptr || sourceMaterialIndex < 0
     || sourceMaterialIndex >= static_cast<int>(m_materials.size()))
  {
    return false;
  }

  UsdGeomPrimvar colorPrimvar = UsdGeomPrimvarsAPI(prim).GetPrimvar(primvarName);
  if(!colorPrimvar)
  {
    return false;
  }

  VtArray<GfVec3f> colors;
  if(!colorPrimvar.Get(&colors) || colors.empty())
  {
    return false;
  }

  MaterialObj mat = m_materials[sourceMaterialIndex];
  const GfVec3f color = colors[0];
  mat.diffuse = glm::vec3(color[0], color[1], color[2]);
  mat.baseColorFactor = mat.diffuse;
  mat.ambient = mat.diffuse * 0.1f;

  m_materials.push_back(mat);
  *materialIndex = static_cast<int>(m_materials.size() - 1);
  return true;
}

void UsdLoader::loadModel(const std::string& filename)
{
  // 打开 USD 舞台
  UsdStageRefPtr stage = UsdStage::Open(filename);
  if(!stage)
  {
    std::cerr << "无法打开 USD 文件: " << filename << std::endl;
    return;
  }

  // 清空现有数据
  m_vertices.clear();
  m_indices.clear();
  m_materials.clear();
  m_textures.clear();
  m_matIndx.clear();
  m_materialIndexMap.clear();
  m_materialBaseColorPrimvars.clear();

  // 第一遍遍历：加载几何和材质
  for(const auto& prim : stage->Traverse())
  {
    if(!prim.IsActive())
    {
      continue;
    }
    if(prim.IsA<UsdGeomMesh>())
    {
      UsdGeomMesh mesh(prim);
      loadIndices(mesh);
      loadVertices(mesh);
      loadTexCoords(mesh);
      loadNormals(mesh);
    }
    if(prim.IsA<UsdShadeMaterial>())
    {
      loadMaterial(prim);
      m_materialIndexMap[prim.GetPath()] = static_cast<int>(m_materials.size() - 1);
    }
  }

  // 第二遍遍历：为每个网格分配材质索引
  for(const pxr::UsdPrim& prim : stage->Traverse())
  {
    if(prim.IsA<pxr::UsdGeomMesh>())
    {
      // 获取材质索引
      int                             materialIndex = 0;  // 默认无材质
      pxr::UsdShadeMaterialBindingAPI bindingAPI(prim);
      pxr::UsdShadeMaterial           material = bindingAPI.ComputeBoundMaterial();
      if(material)
      {
        pxr::SdfPath materialPath = material.GetPrim().GetPath();
        auto         it           = m_materialIndexMap.find(materialPath);
        if(it != m_materialIndexMap.end())
        {
          materialIndex = it->second;
          const auto primvarIt = m_materialBaseColorPrimvars.find(materialPath);
          if(primvarIt != m_materialBaseColorPrimvars.end())
          {
            applyMaterialColorPrimvar(prim, primvarIt->second, it->second, &materialIndex);
          }
        }
      }

      // 计算面数（考虑四边形拆分）
      pxr::UsdGeomMesh mesh(prim);
      pxr::VtIntArray  faceVertexCounts;
      size_t           faceCount = 0;
      if(mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts))
      {
        for(int count : faceVertexCounts)
        {
          if(count == 3)
          {
            faceCount += 1;
          }
          else if(count == 4)
          {
            faceCount += 2;  // 四边形拆分为两个三角形
          }
        }
      }

      // 分配材质索引
      assignMaterialIndices(prim, faceCount, materialIndex);
    }
  }

  // 如果没有材质，添加默认材质
  if(m_materials.empty())
  {
    m_materials.emplace_back(MaterialObj());
  }

  for(auto v : m_vertices)
  {
    v.texCoord.y = 1.0 - v.texCoord.y;
  }
}

void UsdLoader::assignMaterialIndices(const pxr::UsdPrim& prim, size_t faceCount, int materialIndex)
{
  // 为当前网格的每个面分配材质索引
  // 考虑四边形拆分为两个三角形的情况
  for(size_t i = 0; i < faceCount; ++i)
  {
    m_matIndx.push_back(materialIndex);
  }
}
