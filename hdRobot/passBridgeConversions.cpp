#include "passBridgeConversions.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

namespace
{

template <typename MaterialT>
MaterialT ConvertHydraMaterialFields(const HydraMaterial &material)
{
  MaterialT result;
  result.ambient = material.ambient;
  result.diffuse = material.diffuse;
  result.specular = material.specular;
  result.transmittance = material.transmittance;
  result.emission = material.emission;
  result.baseColorFactor = material.baseColorFactor;
  result.emissionFactor = material.emissionFactor;
  result.transmissionColorFactor = material.transmissionColorFactor;
  result.subsurfaceColorFactor = material.subsurfaceColorFactor;
  result.shininess = material.shininess;
  result.ior = material.ior;
  result.opaque = material.opaque;
  result.metallicFactor = material.metallicFactor;
  result.roughnessFactor = material.roughnessFactor;
  result.opacityFactor = material.opacityFactor;
  result.transmissionFactor = material.transmissionFactor;
  result.subsurfaceFactor = material.subsurfaceFactor;
  result.subsurfaceScale = material.subsurfaceScale;
  result.illum = material.illum;
  result.diffuseTextureId = material.diffuseTextureId;
  result.baseColorTextureId = material.baseColorTextureId;
  result.metallicTextureId = material.metallicTextureId;
  result.roughnessTextureId = material.roughnessTextureId;
  result.normalTextureId = material.normalTextureId;
  result.emissionTextureId = material.emissionTextureId;
  result.opacityTextureId = material.opacityTextureId;
  result.subsurfaceTextureId = material.subsurfaceTextureId;
  return result;
}

glm::vec2 GetMeshTexCoord(const HydraMesh &mesh, size_t vertexIndex, bool useAuthoredTexCoords)
{
  if(useAuthoredTexCoords)
  {
    return glm::vec2(mesh.texCoords[vertexIndex][0], 1 - mesh.texCoords[vertexIndex][1]);
  }

  return glm::vec2((mesh.points[vertexIndex][0] + 1.0f) * 0.5f,
                   1 - ((mesh.points[vertexIndex][2] + 1.0f) * 0.5f));
}

}  // namespace

::Material ToMaterial(const HydraMaterial &material)
{
  return ConvertHydraMaterialFields<::Material>(material);
}

void ConvertHydraMeshToGeometry(const HydraMesh &mesh, ::MeshGeometry &geometry)
{
  geometry.vertices.clear();
  geometry.indices.clear();
  geometry.materialIndices.clear();

  const auto &points = mesh.points;
  const auto &normals = mesh.normals;
  const auto &tangents = mesh.tangents;
  const auto &bitangentSigns = mesh.bitangentSigns;
  const auto &faces = mesh.faces;
  const auto &matIdx = mesh.materialIds;
  const bool useAuthoredTexCoords = !mesh.texCoords.empty() && mesh.texCoords.size() == points.size();

  if(points.size() != normals.size())
  {
    std::cerr << "points:" << points.size() << '\n';
    std::cerr << "normals:" << normals.size() << '\n';
    std::cerr << "texCoords:" << points.size() << '\n';
    std::cerr << "Error: points, normals, texCoords size mismatch" << '\n';
    return;
  }

  geometry.vertices.reserve(points.size());
  for(size_t i = 0; i < points.size(); ++i)
  {
    MeshVertex vertex;
    vertex.pos = glm::vec3(points[i][0], points[i][1], points[i][2]);
    vertex.nrm = glm::vec3(normals[i][0], normals[i][1], normals[i][2]);
    vertex.texCoord = GetMeshTexCoord(mesh, i, useAuthoredTexCoords);
    if(tangents.size() == points.size() && bitangentSigns.size() == points.size())
    {
      vertex.tangent = glm::vec4(tangents[i][0], tangents[i][1], tangents[i][2], bitangentSigns[i]);
    }
    vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
    geometry.vertices.push_back(vertex);
  }

  geometry.indices.reserve(faces.size() * 3);
  for(const auto &face : faces)
  {
    geometry.indices.push_back(static_cast<uint32_t>(face[0]));
    geometry.indices.push_back(static_cast<uint32_t>(face[1]));
    geometry.indices.push_back(static_cast<uint32_t>(face[2]));
  }

  geometry.materialIndices.reserve(matIdx.size());
  for(const auto &matId : matIdx)
  {
    geometry.materialIndices.push_back(static_cast<uint32_t>(matId));
  }
}

::CameraSpec ToCameraSpec(const CameraData &camera)
{
  ::CameraSpec result;
  result.name = camera.name;
  result.position = camera.position;
  result.forward = camera.forward;
  result.up = camera.up;
  result.vfov_deg = camera.vfov_deg;
  result.hfov_deg = camera.hfov_deg;
  result.clipStart = camera.clipStart;
  result.clipEnd = camera.clipEnd;
  result.conformPolicy = camera.conformPolicy;
  return result;
}

std::vector<::CameraSpec> ToCameraSpec(const std::vector<CameraData> &cameras)
{
  std::vector<::CameraSpec> result;
  result.reserve(cameras.size());
  for(const CameraData &camera : cameras)
  {
    result.push_back(ToCameraSpec(camera));
  }
  return result;
}

::CameraConformPolicy ToCameraConformPolicy(CameraUtilConformWindowPolicy policy)
{
  switch(policy)
  {
    case CameraUtilMatchVertically:
      return CameraConformPolicy::MatchVertically;
    case CameraUtilMatchHorizontally:
      return CameraConformPolicy::MatchHorizontally;
    case CameraUtilFit:
      return CameraConformPolicy::Fit;
    case CameraUtilCrop:
      return CameraConformPolicy::Crop;
    case CameraUtilDontConform:
      return CameraConformPolicy::DontConform;
  }
  return CameraConformPolicy::Fit;
}

::Light ToLight(const HydraLight &light)
{
  ::Light result;
  result.type = light.type;
  result.textureID = light.textureID;
  result.baseEmission = light.baseEmission;
  result.diffuse = light.diffuse;
  result.specular = light.specular;
  result.direction = light.direction;
  result.angle = light.angle;
  result.position = light.position;
  result.radius = light.radius;
  result.rotateQuat = light.rotateQuat;
  return result;
}

::LidarSensorSpec ToLidarSensorSpec(const LidarSensorData &sensor)
{
  ::LidarSensorSpec result;
  result.name = sensor.name;
  result.position = sensor.camera.position;
  result.forward = sensor.camera.forward;
  result.up = sensor.camera.up;
  result.params.azimuthStartDeg = sensor.params.azimuthStartDeg;
  result.params.azimuthEndDeg = sensor.params.azimuthEndDeg;
  result.params.azimuthStepDeg = sensor.params.azimuthStepDeg;
  result.params.verticalStartDeg = sensor.params.verticalStartDeg;
  result.params.verticalEndDeg = sensor.params.verticalEndDeg;
  result.params.verticalStepDeg = sensor.params.verticalStepDeg;
  result.params.maxRange = sensor.params.maxRange;
  result.params.intensity = sensor.params.intensity;
  return result;
}

std::vector<::LidarSensorSpec> ToLidarSensorSpec(const std::vector<LidarSensorData> &sensors)
{
  std::vector<::LidarSensorSpec> result;
  result.reserve(sensors.size());
  for(const LidarSensorData &sensor : sensors)
  {
    result.push_back(ToLidarSensorSpec(sensor));
  }
  return result;
}

::HeightScanSensorSpec ToHeightScanSensorSpec(const HeightScanSensorData &sensor)
{
  ::HeightScanSensorSpec result;
  result.name = sensor.name;
  result.position = sensor.camera.position;
  result.forward = sensor.camera.forward;
  result.up = sensor.camera.up;
  result.params.uStart = sensor.params.uStart;
  result.params.uEnd = sensor.params.uEnd;
  result.params.uStep = sensor.params.uStep;
  result.params.vStart = sensor.params.vStart;
  result.params.vEnd = sensor.params.vEnd;
  result.params.vStep = sensor.params.vStep;
  result.params.gravityDirectionWs = sensor.params.gravityDirectionWs;
  result.params.maxRange = sensor.params.maxRange;
  return result;
}

std::vector<::HeightScanSensorSpec> ToHeightScanSensorSpec(
    const std::vector<HeightScanSensorData> &sensors)
{
  std::vector<::HeightScanSensorSpec> result;
  result.reserve(sensors.size());
  for(const HeightScanSensorData &sensor : sensors)
  {
    result.push_back(ToHeightScanSensorSpec(sensor));
  }
  return result;
}

::LidarVisualizationConfig ToLidarVisualizationConfig(const LidarVisualizationConfig &config)
{
  ::LidarVisualizationConfig result;
  result.enabled = config.enabled;
  result.visualizeAllSensors = config.visualizeAllSensors;
  result.sensorIndex = config.sensorIndex;
  result.pointSizePixels = config.pointSizePixels;
  return result;
}

::HeightScanVisualizationConfig ToHeightScanVisualizationConfig(
    const HeightScanVisualizationConfig &config)
{
  ::HeightScanVisualizationConfig result;
  result.enabled = config.enabled;
  result.sensorIndex = config.sensorIndex;
  result.pointSizePixels = config.pointSizePixels;
  result.visualizeAllSensors = config.visualizeAllSensors;
  return result;
}

::TileAtlasConfig ToTileAtlasConfig(const TileConfig &config)
{
  ::TileAtlasConfig result;
  result.enabled = config.enabled;
  result.colorEnabled = config.colorEnabled;
  result.depthEnabled = config.depthEnabled;
  result.cameraWidth = config.cameraWidth;
  result.cameraHeight = config.cameraHeight;
  result.gridColumns = config.gridColumns;
  result.gridRows = config.gridRows;
  result.sanitize();
  return result;
}

::RendererOutputConfig ToRendererOutputConfig(
    const TileConfig &tileConfig,
    TileAovChannelMask requestedTileAovChannels,
    const std::vector<LidarSensorData> &lidarSensors,
    const LidarVisualizationConfig &lidarVisualizationConfig,
    const std::vector<HeightScanSensorData> &heightScanSensors,
    const HeightScanVisualizationConfig &heightScanVisualizationConfig)
{
  ::RendererOutputConfig result;
  result.tile.atlas = ToTileAtlasConfig(tileConfig);
  result.tile.requestedChannels = requestedTileAovChannels;
  result.lidar.sensors = ToLidarSensorSpec(lidarSensors);
  result.lidar.visualization = ToLidarVisualizationConfig(lidarVisualizationConfig);
  result.heightScan.sensors = ToHeightScanSensorSpec(heightScanSensors);
  result.heightScan.visualization = ToHeightScanVisualizationConfig(heightScanVisualizationConfig);
  return result;
}

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
