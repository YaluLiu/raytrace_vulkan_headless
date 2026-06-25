#pragma once

#include "UsdRaySensor/heightScanSensor.h"
#include "UsdRaySensor/lidarSensor.h"
#include "training_scene.h"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>

namespace headless_training
{

bool IsLidarSensorPrim(const PXR_NS::UsdPrim& prim);
bool IsHeightScanSensorPrim(const PXR_NS::UsdPrim& prim);

LidarParams SanitizeLidarParams(LidarParams params);
HeightScanParams SanitizeHeightScanParams(HeightScanParams params);

LidarSensorSpec ReadLidarSensor(const PXR_NS::UsdGeomLidarSensor& sensor, PXR_NS::UsdTimeCode time);
HeightScanSensorSpec ReadHeightScanSensor(const PXR_NS::UsdGeomHeightScanSensor& sensor, PXR_NS::UsdTimeCode time);

} // namespace headless_training
