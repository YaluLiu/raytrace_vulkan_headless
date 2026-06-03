#pragma once

#include "renderParam.h"

#include <engine/mesh_types.hpp>
#include <engine/output_config.hpp>
#include <engine/renderer_types.hpp>

#include <pxr/imaging/cameraUtil/conformWindow.h>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

Material ToMaterial(const HydraMaterial &material);
void ConvertHydraMeshToGeometry(const HydraMesh &mesh, MeshGeometry &geometry);

CameraSpec ToCameraSpec(const HdRobotCameraData &camera);
std::vector<CameraSpec> ToCameraSpec(const std::vector<HdRobotCameraData> &cameras);
CameraConformPolicy ToCameraConformPolicy(CameraUtilConformWindowPolicy policy);

Light ToLight(const HydraLight &light);

RendererOutputConfig ToRendererOutputConfig(
    const HdRobotTileConfig &tileConfig,
    TileAovChannelMask requestedTileAovChannels,
    const std::vector<HdRobotLidarSensorData> &lidarSensors,
    const HdRobotLidarVisualizationConfig &lidarVisualizationConfig,
    const std::vector<HdRobotHeightScanSensorData> &heightScanSensors,
    const HdRobotHeightScanVisualizationConfig &heightScanVisualizationConfig);

PXR_NAMESPACE_CLOSE_SCOPE
