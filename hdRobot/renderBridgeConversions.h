#pragma once

#include "renderParam.h"

#include <engine/height_scan_types.hpp>
#include <engine/lidar_types.hpp>
#include <engine/mesh_types.hpp>
#include <engine/renderer_types.hpp>
#include <engine/tile_config.hpp>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

Material ToMaterial(const HydraMaterial &material);
void ConvertHydraMeshToGeometry(const HydraMesh &mesh, MeshGeometry &geometry);

CameraSpec ToCameraSpec(const HdRobotCameraData &camera);
std::vector<CameraSpec> ToCameraSpec(const std::vector<HdRobotCameraData> &cameras);

Light ToLight(const HydraLight &light);

LidarSensorSpec ToLidarSensorSpec(const HdRobotLidarSensorData &sensor);
std::vector<LidarSensorSpec> ToLidarSensorSpec(const std::vector<HdRobotLidarSensorData> &sensors);

HeightScanSensorSpec ToHeightScanSensorSpec(const HdRobotHeightScanSensorData &sensor);
std::vector<HeightScanSensorSpec> ToHeightScanSensorSpec(
    const std::vector<HdRobotHeightScanSensorData> &sensors);

LidarVisualizationConfig ToLidarVisualizationConfig(const HdRobotLidarVisualizationConfig &config);
HeightScanVisualizationConfig ToHeightScanVisualizationConfig(
    const HdRobotHeightScanVisualizationConfig &config);

TileAtlasConfig ToTileAtlasConfig(const HdRobotTileConfig &config);

PXR_NAMESPACE_CLOSE_SCOPE
