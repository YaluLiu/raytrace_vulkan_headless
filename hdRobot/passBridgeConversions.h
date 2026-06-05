#pragma once

#include "camera.h"
#include "heightScanSensor.h"
#include "lidarSensor.h"
#include "renderParam.h"
#include "sceneData.h"

#include <engine/mesh_types.hpp>
#include <engine/output_config.hpp>
#include <engine/renderer_types.hpp>

#include <pxr/imaging/cameraUtil/conformWindow.h>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

::Material ToMaterial(const HydraMaterial &material);
void ConvertHydraMeshToGeometry(const HydraMesh &mesh, ::MeshGeometry &geometry);

::CameraSpec ToCameraSpec(const CameraData &camera);
std::vector<::CameraSpec> ToCameraSpec(const std::vector<CameraData> &cameras);
::CameraConformPolicy ToCameraConformPolicy(CameraUtilConformWindowPolicy policy);

::Light ToLight(const HydraLight &light);

::RendererOutputConfig ToRendererOutputConfig(
    const TileConfig &tileConfig,
    TileAovChannelMask requestedTileAovChannels,
    const std::vector<LidarSensorData> &lidarSensors,
    const LidarVisualizationConfig &lidarVisualizationConfig,
    const std::vector<HeightScanSensorData> &heightScanSensors,
    const HeightScanVisualizationConfig &heightScanVisualizationConfig);

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
