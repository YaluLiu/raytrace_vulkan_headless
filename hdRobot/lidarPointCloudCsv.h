#pragma once

#include <engine/lidar_types.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE
namespace hdrobot {

size_t CountLidarPoints(const ::LidarFramePointCloud& frame);

::LidarFramePointCloud SelectLidarPointCloudSensors(const ::LidarFramePointCloud& frame,
                                                    bool allSensors,
                                                    uint32_t sensorIndex);

bool WriteLidarPointCloudCsv(const ::LidarFramePointCloud& frame,
                             uint64_t csvFrameId,
                             const std::string& filePath,
                             std::string* errorMessage);

} // namespace hdrobot
PXR_NAMESPACE_CLOSE_SCOPE
