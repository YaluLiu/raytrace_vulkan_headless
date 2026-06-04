#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

void HdRobotRenderParam::SetTileConfig(const HdRobotTileConfig& config)
{
  std::lock_guard guard(mutex);
  tileConfig = config;
}

HdRobotTileConfig HdRobotRenderParam::GetTileConfig() const
{
  std::lock_guard guard(mutex);
  return tileConfig;
}

void HdRobotRenderParam::SetLidarVisualizationConfig(const HdRobotLidarVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  lidarVisualizationConfig = config;
}

HdRobotLidarVisualizationConfig HdRobotRenderParam::GetLidarVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return lidarVisualizationConfig;
}

void HdRobotRenderParam::SetHeightScanVisualizationConfig(const HdRobotHeightScanVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  heightScanVisualizationConfig = config;
}

HdRobotHeightScanVisualizationConfig HdRobotRenderParam::GetHeightScanVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return heightScanVisualizationConfig;
}

PXR_NAMESPACE_CLOSE_SCOPE
