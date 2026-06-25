#include "usd_mesh_reader.h"

#include "usd_material_reader.h"
#include "usd_scene_reader_utils.h"

#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <array>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include <glm/gtx/norm.hpp>

namespace headless_training
{
namespace
{

glm::vec3 ComputeTriangleNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
  const glm::vec3 normal = glm::cross(b - a, c - a);
  if(glm::length2(normal) < kSensorEpsilon * kSensorEpsilon)
  {
    return glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return glm::normalize(normal);
}

glm::vec2 DefaultTexCoord(glm::vec3 position)
{
  return glm::vec2((position.x + 1.0f) * 0.5f, 1.0f - ((position.z + 1.0f) * 0.5f));
}

glm::vec2 ToEngineTexCoord(const PXR_NS::GfVec2f& texCoord)
{
  return glm::vec2(texCoord[0], 1.0f - texCoord[1]);
}

std::optional<glm::vec2> ReadAuthoredTexCoord(const TexCoordPrimvar& texCoords,
                                              int pointIndex,
                                              size_t faceIndex,
                                              int faceVertexStart,
                                              int faceCornerIndex)
{
  size_t texCoordIndex = std::numeric_limits<size_t>::max();
  if(texCoords.interpolation == PXR_NS::UsdGeomTokens->faceVarying)
  {
    texCoordIndex = static_cast<size_t>(faceVertexStart + faceCornerIndex);
  }
  else if(texCoords.interpolation == PXR_NS::UsdGeomTokens->uniform)
  {
    texCoordIndex = faceIndex;
  }
  else if(texCoords.interpolation == PXR_NS::UsdGeomTokens->constant)
  {
    texCoordIndex = 0;
  }
  else
  {
    texCoordIndex = static_cast<size_t>(pointIndex);
  }

  if(texCoordIndex >= texCoords.values.size())
  {
    return std::nullopt;
  }
  return ToEngineTexCoord(texCoords.values[texCoordIndex]);
}

std::array<glm::vec2, 3> ResolveTriangleTexCoords(const PXR_NS::VtVec3fArray& points,
                                                  const std::optional<TexCoordPrimvar>& texCoords,
                                                  size_t faceIndex,
                                                  int faceVertexStart,
                                                  const int pointIndices[3],
                                                  const int faceCornerIndices[3])
{
  std::array<glm::vec2, 3> result{
      DefaultTexCoord(ToGlm(points[static_cast<size_t>(pointIndices[0])])),
      DefaultTexCoord(ToGlm(points[static_cast<size_t>(pointIndices[1])])),
      DefaultTexCoord(ToGlm(points[static_cast<size_t>(pointIndices[2])]))};
  if(!texCoords)
  {
    return result;
  }

  for(size_t corner = 0; corner < result.size(); ++corner)
  {
    const std::optional<glm::vec2> authored =
        ReadAuthoredTexCoord(*texCoords, pointIndices[corner], faceIndex, faceVertexStart, faceCornerIndices[corner]);
    if(authored)
    {
      result[corner] = *authored;
    }
  }
  return result;
}

void AppendTriangle(MeshGeometry& geometry,
                    const PXR_NS::VtVec3fArray& points,
                    const std::array<glm::vec3, 3>& cornerNormals,
                    const std::array<glm::vec2, 3>& cornerTexCoords,
                    int i0,
                    int i1,
                    int i2)
{
  const glm::vec3 p0 = ToGlm(points[static_cast<size_t>(i0)]);
  const glm::vec3 p1 = ToGlm(points[static_cast<size_t>(i1)]);
  const glm::vec3 p2 = ToGlm(points[static_cast<size_t>(i2)]);
  const glm::vec3 faceNormal = ComputeTriangleNormal(p0, p1, p2);
  const glm::vec3 positions[3] = {p0, p1, p2};

  for(int corner = 0; corner < 3; ++corner)
  {
    MeshVertex vertex;
    vertex.pos = positions[corner];
    vertex.nrm = NormalizeOr(cornerNormals[static_cast<size_t>(corner)], faceNormal);
    vertex.color = glm::vec3(1.0f);
    vertex.texCoord = cornerTexCoords[static_cast<size_t>(corner)];
    geometry.indices.push_back(static_cast<uint32_t>(geometry.vertices.size()));
    geometry.vertices.push_back(vertex);
  }
  geometry.materialIndices.push_back(0);
}

std::optional<glm::vec3> ReadAuthoredNormal(const PXR_NS::VtVec3fArray& normals,
                                            const PXR_NS::TfToken& interpolation,
                                            int pointIndex,
                                            size_t faceIndex,
                                            int faceVertexStart,
                                            int faceCornerIndex)
{
  if(normals.empty())
  {
    return std::nullopt;
  }

  size_t normalIndex = std::numeric_limits<size_t>::max();
  if(interpolation == PXR_NS::UsdGeomTokens->faceVarying)
  {
    normalIndex = static_cast<size_t>(faceVertexStart + faceCornerIndex);
  }
  else if(interpolation == PXR_NS::UsdGeomTokens->uniform)
  {
    normalIndex = faceIndex;
  }
  else if(interpolation == PXR_NS::UsdGeomTokens->constant)
  {
    normalIndex = 0;
  }
  else
  {
    normalIndex = static_cast<size_t>(pointIndex);
  }

  if(normalIndex >= normals.size())
  {
    return std::nullopt;
  }
  return ToGlm(normals[normalIndex]);
}

std::array<glm::vec3, 3> ResolveTriangleNormals(const PXR_NS::VtVec3fArray& points,
                                                const PXR_NS::VtVec3fArray& normals,
                                                const PXR_NS::TfToken& interpolation,
                                                size_t faceIndex,
                                                int faceVertexStart,
                                                const int pointIndices[3],
                                                const int faceCornerIndices[3])
{
  const glm::vec3 p0 = ToGlm(points[static_cast<size_t>(pointIndices[0])]);
  const glm::vec3 p1 = ToGlm(points[static_cast<size_t>(pointIndices[1])]);
  const glm::vec3 p2 = ToGlm(points[static_cast<size_t>(pointIndices[2])]);
  const glm::vec3 fallback = ComputeTriangleNormal(p0, p1, p2);

  std::array<glm::vec3, 3> result{fallback, fallback, fallback};
  for(size_t corner = 0; corner < result.size(); ++corner)
  {
    const std::optional<glm::vec3> authored =
        ReadAuthoredNormal(normals, interpolation, pointIndices[corner], faceIndex, faceVertexStart,
                           faceCornerIndices[corner]);
    if(authored)
    {
      result[corner] = *authored;
    }
  }
  return result;
}

} // namespace

std::optional<TexCoordPrimvar> ReadTexCoordPrimvar(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time)
{
  const PXR_NS::TfToken texCoordNames[] = {PXR_NS::TfToken("st"),   PXR_NS::TfToken("st0"),
                                           PXR_NS::TfToken("st_0"), PXR_NS::TfToken("st1"),
                                           PXR_NS::TfToken("st_1"), PXR_NS::TfToken("UV0"),
                                           PXR_NS::TfToken("UV1")};

  const PXR_NS::UsdGeomPrimvarsAPI primvarsApi(mesh.GetPrim());
  for(const PXR_NS::TfToken& name : texCoordNames)
  {
    const PXR_NS::UsdGeomPrimvar primvar = primvarsApi.GetPrimvar(name);
    if(!primvar)
    {
      continue;
    }

    PXR_NS::VtVec2fArray values;
    if(!primvar.ComputeFlattened(&values, time) || values.empty())
    {
      continue;
    }

    TexCoordPrimvar result;
    result.values = std::move(values);
    result.interpolation = primvar.GetInterpolation();
    return result;
  }
  return std::nullopt;
}

MeshGeometry ReadMeshGeometry(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time)
{
  PXR_NS::VtVec3fArray points;
  PXR_NS::VtIntArray faceVertexCounts;
  PXR_NS::VtIntArray faceVertexIndices;
  PXR_NS::VtVec3fArray normals;

  if(!mesh.GetPointsAttr().Get(&points, time) || points.empty())
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has no points");
  }
  if(!mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts, time) || faceVertexCounts.empty())
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has no faceVertexCounts");
  }
  if(!mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices, time) || faceVertexIndices.empty())
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has no faceVertexIndices");
  }
  mesh.GetNormalsAttr().Get(&normals, time);
  const PXR_NS::TfToken normalsInterpolation = mesh.GetNormalsInterpolation();
  const std::optional<TexCoordPrimvar> texCoords = ReadTexCoordPrimvar(mesh, time);

  MeshGeometry geometry;
  int indexCursor = 0;
  for(size_t faceIndex = 0; faceIndex < faceVertexCounts.size(); ++faceIndex)
  {
    const int count = faceVertexCounts[faceIndex];
    if(count < 3)
    {
      throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " contains a face with fewer than 3 vertices");
    }
    if(indexCursor + count > static_cast<int>(faceVertexIndices.size()))
    {
      throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " faceVertexIndices are shorter than counts");
    }
    if(count > 4)
    {
      std::cerr << "[robot_training_headless] Warning: fan triangulating n-gon face " << faceIndex << " on "
                << mesh.GetPath().GetString() << "; only simple convex faces are supported\n";
    }

    for(int i = 1; i + 1 < count; ++i)
    {
      const int tri[3] = {
          faceVertexIndices[static_cast<size_t>(indexCursor)],
          faceVertexIndices[static_cast<size_t>(indexCursor + i)],
          faceVertexIndices[static_cast<size_t>(indexCursor + i + 1)],
      };
      const int faceCornerIndices[3] = {0, i, i + 1};
      for(const int pointIndex : tri)
      {
        if(pointIndex < 0 || pointIndex >= static_cast<int>(points.size()))
        {
          throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has out-of-range point index");
        }
      }
      const std::array<glm::vec3, 3> cornerNormals =
          ResolveTriangleNormals(points, normals, normalsInterpolation, faceIndex, indexCursor, tri, faceCornerIndices);
      const std::array<glm::vec2, 3> cornerTexCoords =
          ResolveTriangleTexCoords(points, texCoords, faceIndex, indexCursor, tri, faceCornerIndices);
      AppendTriangle(geometry, points, cornerNormals, cornerTexCoords, tri[0], tri[1], tri[2]);
    }
    indexCursor += count;
  }
  if(indexCursor != static_cast<int>(faceVertexIndices.size()))
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has unused faceVertexIndices");
  }
  return geometry;
}

TrainingMeshInstance ReadMesh(const PXR_NS::UsdGeomMesh& mesh, TextureRegistry& textureRegistry,
                              PXR_NS::UsdTimeCode time)
{
  TrainingMeshInstance result;
  result.name = mesh.GetPath().GetString();
  result.geometry = ReadMeshGeometry(mesh, time);
  result.materials.push_back(ReadMeshMaterial(mesh, textureRegistry, time));
  result.worldTransform = ToEngineTransform(PXR_NS::UsdGeomXformable(mesh.GetPrim()).ComputeLocalToWorldTransform(time));
  result.visible = !IsInvisible(mesh.GetPrim(), time);
  result.traceMask = ReadTraceMask(mesh.GetPrim());
  return result;
}

} // namespace headless_training
