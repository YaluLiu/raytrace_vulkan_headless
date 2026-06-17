#include "usd_scene_loader.h"

#include "UsdRaySensor/heightScanSensor.h"
#include "UsdRaySensor/lidarSensor.h"
#include "UsdRaySensor/tokens.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace headless_training
{
namespace
{
constexpr float kSensorEpsilon = 1.0e-6f;
constexpr float kParamEpsilon = 1.0e-4f;

glm::vec3 ToGlm(const PXR_NS::GfVec3f& value)
{
  return glm::vec3(value[0], value[1], value[2]);
}

glm::vec3 ToGlm(const PXR_NS::GfVec3d& value)
{
  return glm::vec3(static_cast<float>(value[0]), static_cast<float>(value[1]), static_cast<float>(value[2]));
}

std::array<double, 16> ToRowMajorArray(const PXR_NS::GfMatrix4d& matrix)
{
  std::array<double, 16> values{};
  for(int row = 0; row < 4; ++row)
  {
    for(int column = 0; column < 4; ++column)
    {
      values[static_cast<size_t>(row * 4 + column)] = matrix[row][column];
    }
  }
  return values;
}

glm::mat4 ToEngineTransform(const PXR_NS::GfMatrix4d& matrix)
{
  return MakeEngineTransformFromUsdRows(ToRowMajorArray(matrix));
}

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback)
{
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
     glm::length2(value) < kSensorEpsilon * kSensorEpsilon)
  {
    value = fallback;
  }
  if(glm::length2(value) < kSensorEpsilon * kSensorEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

CameraSpec MakePoseFromTransform(const std::string& name, const PXR_NS::GfMatrix4d& transform)
{
  const PXR_NS::GfVec3d position = transform.Transform(PXR_NS::GfVec3d(0.0, 0.0, 0.0));
  PXR_NS::GfVec3d forward = transform.TransformDir(PXR_NS::GfVec3d(0.0, 0.0, -1.0));
  PXR_NS::GfVec3d up = transform.TransformDir(PXR_NS::GfVec3d(0.0, 1.0, 0.0));
  forward.Normalize();
  up.Normalize();

  CameraSpec spec;
  spec.name = name;
  spec.position = ToGlm(position);
  spec.forward = NormalizeOr(ToGlm(forward), glm::vec3(0.0f, 0.0f, -1.0f));
  spec.up = NormalizeOr(ToGlm(up), glm::vec3(0.0f, 1.0f, 0.0f));
  if(glm::length2(glm::cross(spec.forward, spec.up)) < kSensorEpsilon * kSensorEpsilon)
  {
    spec.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return spec;
}

float ClampPositive(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, kParamEpsilon);
}

float ClampNonNegative(float value, float fallback)
{
  return std::max(std::isfinite(value) ? value : fallback, 0.0f);
}

float ClampStep(float value, float fallback)
{
  return std::max(std::fabs(std::isfinite(value) ? value : fallback), kParamEpsilon);
}

LidarParams SanitizeLidarParams(LidarParams params)
{
  LidarParams defaults;
  params.azimuthStepDeg = ClampStep(params.azimuthStepDeg, defaults.azimuthStepDeg);
  params.verticalStepDeg = ClampStep(params.verticalStepDeg, defaults.verticalStepDeg);
  params.maxRange = ClampPositive(params.maxRange, defaults.maxRange);
  params.intensity = ClampNonNegative(params.intensity, defaults.intensity);
  return params;
}

HeightScanParams SanitizeHeightScanParams(HeightScanParams params)
{
  HeightScanParams defaults;
  params.uStep = ClampStep(params.uStep, defaults.uStep);
  params.vStep = ClampStep(params.vStep, defaults.vStep);
  params.gravityDirectionWs = NormalizeOr(params.gravityDirectionWs, defaults.gravityDirectionWs);
  params.maxRange = ClampPositive(params.maxRange, defaults.maxRange);
  return params;
}

bool IsInvisible(const PXR_NS::UsdPrim& prim, PXR_NS::UsdTimeCode time)
{
  PXR_NS::UsdGeomImageable imageable(prim);
  if(!imageable)
  {
    return false;
  }
  return imageable.ComputeVisibility(time) == PXR_NS::UsdGeomTokens->invisible;
}

uint32_t ReadTraceMask(const PXR_NS::UsdPrim& prim)
{
  const PXR_NS::UsdAttribute attr = prim.GetAttribute(PXR_NS::TfToken("hdRobot:traceRole"));
  if(!attr)
  {
    return kTraceMaskDefaultGeometry;
  }

  PXR_NS::VtValue value;
  if(!attr.Get(&value))
  {
    return kTraceMaskDefaultGeometry;
  }

  if(value.IsHolding<PXR_NS::TfToken>() && value.UncheckedGet<PXR_NS::TfToken>() == PXR_NS::TfToken("ground"))
  {
    return kTraceMaskGround;
  }
  if(value.IsHolding<std::string>() && value.UncheckedGet<std::string>() == "ground")
  {
    return kTraceMaskGround;
  }

  return kTraceMaskDefaultGeometry;
}

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

void AppendTriangle(MeshGeometry& geometry,
                    const PXR_NS::VtVec3fArray& points,
                    const std::array<glm::vec3, 3>& cornerNormals,
                    int i0,
                    int i1,
                    int i2)
{
  const glm::vec3 p0 = ToGlm(points[static_cast<size_t>(i0)]);
  const glm::vec3 p1 = ToGlm(points[static_cast<size_t>(i1)]);
  const glm::vec3 p2 = ToGlm(points[static_cast<size_t>(i2)]);
  const glm::vec3 faceNormal = ComputeTriangleNormal(p0, p1, p2);
  const int indices[3] = {i0, i1, i2};
  const glm::vec3 positions[3] = {p0, p1, p2};

  for(int corner = 0; corner < 3; ++corner)
  {
    MeshVertex vertex;
    vertex.pos = positions[corner];
    vertex.nrm = NormalizeOr(cornerNormals[static_cast<size_t>(corner)], faceNormal);
    vertex.color = glm::vec3(1.0f);
    vertex.texCoord = DefaultTexCoord(vertex.pos);
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
      AppendTriangle(geometry, points, cornerNormals, tri[0], tri[1], tri[2]);
    }
    indexCursor += count;
  }
  if(indexCursor != static_cast<int>(faceVertexIndices.size()))
  {
    throw std::runtime_error("mesh " + mesh.GetPath().GetString() + " has unused faceVertexIndices");
  }
  return geometry;
}

TrainingMeshInstance ReadMesh(const PXR_NS::UsdGeomMesh& mesh, PXR_NS::UsdTimeCode time)
{
  TrainingMeshInstance result;
  result.name = mesh.GetPath().GetString();
  result.geometry = ReadMeshGeometry(mesh, time);
  result.materials.push_back(Material{});
  result.worldTransform = ToEngineTransform(PXR_NS::UsdGeomXformable(mesh.GetPrim()).ComputeLocalToWorldTransform(time));
  result.visible = !IsInvisible(mesh.GetPrim(), time);
  result.traceMask = ReadTraceMask(mesh.GetPrim());
  return result;
}

CameraSpec ReadCamera(const PXR_NS::UsdGeomCamera& camera, PXR_NS::UsdTimeCode time)
{
  CameraSpec result =
      MakePoseFromTransform(camera.GetPath().GetString(),
                            PXR_NS::UsdGeomXformable(camera.GetPrim()).ComputeLocalToWorldTransform(time));

  const PXR_NS::GfCamera gfCamera = camera.GetCamera(time);
  if(gfCamera.GetProjection() == PXR_NS::GfCamera::Perspective && gfCamera.GetFocalLength() > kSensorEpsilon)
  {
    const float verticalAperture = gfCamera.GetVerticalAperture() * PXR_NS::GfCamera::APERTURE_UNIT;
    const float horizontalAperture = gfCamera.GetHorizontalAperture() * PXR_NS::GfCamera::APERTURE_UNIT;
    const float focalLength = gfCamera.GetFocalLength() * PXR_NS::GfCamera::FOCAL_LENGTH_UNIT;
    const float verticalFov = 2.0f * std::atan(verticalAperture / (2.0f * focalLength));
    const float horizontalFov = 2.0f * std::atan(horizontalAperture / (2.0f * focalLength));
    result.verticalFovDegrees = std::clamp(glm::degrees(verticalFov), 1.0f, 179.0f);
    result.horizontalFovDegrees = std::clamp(glm::degrees(horizontalFov), 1.0f, 179.0f);
  }

  const PXR_NS::GfRange1f clippingRange = gfCamera.GetClippingRange();
  result.clipStart = std::max(clippingRange.GetMin(), 0.001f);
  result.clipEnd = std::max(clippingRange.GetMax(), result.clipStart + 0.001f);
  return result;
}

bool IsLidarSensorPrim(const PXR_NS::UsdPrim& prim)
{
  return prim.GetTypeName() == PXR_NS::UsdRaySensorTokens->LidarSensor || prim.IsA<PXR_NS::UsdGeomLidarSensor>();
}

bool IsHeightScanSensorPrim(const PXR_NS::UsdPrim& prim)
{
  return prim.GetTypeName() == PXR_NS::UsdRaySensorTokens->HeightScanSensor ||
         prim.IsA<PXR_NS::UsdGeomHeightScanSensor>();
}

LidarSensorSpec ReadLidarSensor(const PXR_NS::UsdGeomLidarSensor& sensor, PXR_NS::UsdTimeCode time)
{
  CameraSpec pose =
      MakePoseFromTransform(sensor.GetPath().GetString(),
                            PXR_NS::UsdGeomXformable(sensor.GetPrim()).ComputeLocalToWorldTransform(time));
  LidarSensorSpec result;
  result.name = pose.name;
  result.position = pose.position;
  result.forward = pose.forward;
  result.up = pose.up;
  result.params.azimuthStartDeg = sensor.GetAzimuthStartDeg(time);
  result.params.azimuthEndDeg = sensor.GetAzimuthEndDeg(time);
  result.params.azimuthStepDeg = sensor.GetAzimuthStepDeg(time);
  result.params.verticalStartDeg = sensor.GetVerticalStartDeg(time);
  result.params.verticalEndDeg = sensor.GetVerticalEndDeg(time);
  result.params.verticalStepDeg = sensor.GetVerticalStepDeg(time);
  result.params.maxRange = sensor.GetMaxRange(time);
  result.params.intensity = sensor.GetIntensity(time);
  result.params = SanitizeLidarParams(result.params);
  return result;
}

HeightScanSensorSpec ReadHeightScanSensor(const PXR_NS::UsdGeomHeightScanSensor& sensor, PXR_NS::UsdTimeCode time)
{
  CameraSpec pose =
      MakePoseFromTransform(sensor.GetPath().GetString(),
                            PXR_NS::UsdGeomXformable(sensor.GetPrim()).ComputeLocalToWorldTransform(time));
  HeightScanSensorSpec result;
  result.name = pose.name;
  result.position = pose.position;
  result.forward = pose.forward;
  result.up = pose.up;
  result.params.uStart = sensor.GetUStart(time);
  result.params.uEnd = sensor.GetUEnd(time);
  result.params.uStep = sensor.GetUStep(time);
  result.params.vStart = sensor.GetVStart(time);
  result.params.vEnd = sensor.GetVEnd(time);
  result.params.vStep = sensor.GetVStep(time);
  result.params.gravityDirectionWs = ToGlm(sensor.GetGravityDirectionWs(time));
  result.params.maxRange = sensor.GetMaxRange(time);
  result.params = SanitizeHeightScanParams(result.params);
  return result;
}
} // namespace

TrainingSceneDescription LoadUsdTrainingScene(const std::filesystem::path& usdPath, const UsdSceneLoadOptions& options)
{
  PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(usdPath.string());
  if(!stage)
  {
    throw std::runtime_error("failed to open USD stage: " + usdPath.string());
  }

  const PXR_NS::UsdTimeCode time = PXR_NS::UsdTimeCode::Default();
  TrainingSceneDescription result;
  bool cameraPathRequested = !options.cameraPath.empty();
  bool requestedCameraFound = false;

  for(const PXR_NS::UsdPrim& prim : stage->Traverse())
  {
    if(!prim.IsActive())
    {
      continue;
    }

    PXR_NS::UsdGeomMesh mesh(prim);
    if(mesh)
    {
      result.meshes.push_back(ReadMesh(mesh, time));
      continue;
    }

    PXR_NS::UsdGeomCamera camera(prim);
    if(camera)
    {
      const bool selected = !cameraPathRequested || prim.GetPath() == PXR_NS::SdfPath(options.cameraPath);
      if(selected && result.cameras.empty())
      {
        result.cameras.push_back(ReadCamera(camera, time));
        requestedCameraFound = cameraPathRequested;
      }
      continue;
    }

    if(IsLidarSensorPrim(prim))
    {
      PXR_NS::UsdGeomLidarSensor sensor(prim);
      if(sensor.GetEnabled(time))
      {
        result.lidarSensors.push_back(ReadLidarSensor(sensor, time));
      }
      continue;
    }

    if(IsHeightScanSensorPrim(prim))
    {
      PXR_NS::UsdGeomHeightScanSensor sensor(prim);
      if(sensor.GetEnabled(time))
      {
        result.heightScanSensors.push_back(ReadHeightScanSensor(sensor, time));
      }
      continue;
    }
  }

  if(cameraPathRequested && !requestedCameraFound)
  {
    throw std::runtime_error("requested camera path was not found or was not a UsdGeomCamera: " + options.cameraPath);
  }

  return result;
}

} // namespace headless_training
