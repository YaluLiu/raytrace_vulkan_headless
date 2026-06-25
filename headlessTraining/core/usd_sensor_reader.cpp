#include "usd_sensor_reader.h"

#include "UsdRaySensor/tokens.h"
#include "usd_scene_reader_utils.h"

#include <pxr/usd/usdGeom/xformable.h>

namespace headless_training
{

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

} // namespace headless_training
