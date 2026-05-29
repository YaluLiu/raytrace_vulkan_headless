#pragma once

#include "renderParam.h"

#include <raster/height_scan_types.hpp>
#include <raster/lidar_types.hpp>
#include <raster/mesh_types.hpp>
#include <raster/raster_renderer_types.hpp>
#include <raster/tile_config.hpp>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

RasterMaterial ToRasterMaterial(const HydraMaterial &material);
WaveFrontMaterial ToWaveFrontMaterial(const HydraMaterial &material);
void ConvertHydraMeshToRasterGeometry(const HydraMesh &mesh, RasterMeshGeometry &geometry);
RasterMeshUpload ToRasterMeshUpload(const HydraMesh &mesh, const std::vector<HydraMaterial> &materials);

RasterCameraSpec ToRasterCameraSpec(const HdRobotCameraData &camera);
std::vector<RasterCameraSpec> ToRasterCameraSpec(const std::vector<HdRobotCameraData> &cameras);

Light ToRasterLight(const HydraLight &light);

RasterLidarSensorSpec ToRasterLidarSensorSpec(const HdRobotLidarSensorData &sensor);
std::vector<RasterLidarSensorSpec> ToRasterLidarSensorSpec(const std::vector<HdRobotLidarSensorData> &sensors);

RasterHeightScanSensorSpec ToRasterHeightScanSensorSpec(const HdRobotHeightScanSensorData &sensor);
std::vector<RasterHeightScanSensorSpec> ToRasterHeightScanSensorSpec(
    const std::vector<HdRobotHeightScanSensorData> &sensors);

RasterLidarVisualizationConfig ToRasterLidarVisualizationConfig(const HdRobotLidarVisualizationConfig &config);
RasterHeightScanVisualizationConfig ToRasterHeightScanVisualizationConfig(
    const HdRobotHeightScanVisualizationConfig &config);

TileAtlasConfig ToTileAtlasConfig(const HdRobotTileConfig &config);

PXR_NAMESPACE_CLOSE_SCOPE
