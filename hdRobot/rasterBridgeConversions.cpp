#include "rasterBridgeConversions.h"

PXR_NAMESPACE_OPEN_SCOPE

WaveFrontMaterial ToWaveFrontMaterial(const HydraMaterial &material)
{
  WaveFrontMaterial result;
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

RasterCameraSpec ToRasterCameraSpec(const HdRobotCameraData &camera)
{
  RasterCameraSpec result;
  result.name = camera.name;
  result.position = camera.position;
  result.forward = camera.forward;
  result.up = camera.up;
  result.vfov_deg = camera.vfov_deg;
  result.clipStart = camera.clipStart;
  result.clipEnd = camera.clipEnd;
  return result;
}

std::vector<RasterCameraSpec> ToRasterCameraSpec(const std::vector<HdRobotCameraData> &cameras)
{
  std::vector<RasterCameraSpec> result;
  result.reserve(cameras.size());
  for(const HdRobotCameraData &camera : cameras)
  {
    result.push_back(ToRasterCameraSpec(camera));
  }
  return result;
}

RasterLidarSensorSpec ToRasterLidarSensorSpec(const HdRobotLidarSensorData &sensor)
{
  RasterLidarSensorSpec result;
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

std::vector<RasterLidarSensorSpec> ToRasterLidarSensorSpec(const std::vector<HdRobotLidarSensorData> &sensors)
{
  std::vector<RasterLidarSensorSpec> result;
  result.reserve(sensors.size());
  for(const HdRobotLidarSensorData &sensor : sensors)
  {
    result.push_back(ToRasterLidarSensorSpec(sensor));
  }
  return result;
}

RasterHeightScanSensorSpec ToRasterHeightScanSensorSpec(const HdRobotHeightScanSensorData &sensor)
{
  RasterHeightScanSensorSpec result;
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

std::vector<RasterHeightScanSensorSpec> ToRasterHeightScanSensorSpec(
    const std::vector<HdRobotHeightScanSensorData> &sensors)
{
  std::vector<RasterHeightScanSensorSpec> result;
  result.reserve(sensors.size());
  for(const HdRobotHeightScanSensorData &sensor : sensors)
  {
    result.push_back(ToRasterHeightScanSensorSpec(sensor));
  }
  return result;
}

RasterLidarVisualizationConfig ToRasterLidarVisualizationConfig(const HdRobotLidarVisualizationConfig &config)
{
  RasterLidarVisualizationConfig result;
  result.enabled = config.enabled;
  result.visualizeAllSensors = config.visualizeAllSensors;
  result.sensorIndex = config.sensorIndex;
  result.pointSizePixels = config.pointSizePixels;
  return result;
}

RasterHeightScanVisualizationConfig ToRasterHeightScanVisualizationConfig(
    const HdRobotHeightScanVisualizationConfig &config)
{
  RasterHeightScanVisualizationConfig result;
  result.enabled = config.enabled;
  result.sensorIndex = config.sensorIndex;
  result.pointSizePixels = config.pointSizePixels;
  result.visualizeAllSensors = config.visualizeAllSensors;
  return result;
}

TileAtlasConfig ToTileAtlasConfig(const HdRobotTileConfig &config)
{
  TileAtlasConfig result;
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

PXR_NAMESPACE_CLOSE_SCOPE
