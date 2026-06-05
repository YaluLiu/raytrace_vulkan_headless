#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

void RenderParam::SetTileConfig(const TileConfig& config)
{
  std::lock_guard guard(mutex);
  tileConfig = config;
}

TileConfig RenderParam::GetTileConfig() const
{
  std::lock_guard guard(mutex);
  return tileConfig;
}

void RenderParam::SetLidarVisualizationConfig(const LidarVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  lidarVisualizationConfig = config;
}

LidarVisualizationConfig RenderParam::GetLidarVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return lidarVisualizationConfig;
}

void RenderParam::SetHeightScanVisualizationConfig(const HeightScanVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  heightScanVisualizationConfig = config;
}

HeightScanVisualizationConfig RenderParam::GetHeightScanVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return heightScanVisualizationConfig;
}

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
