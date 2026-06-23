#include "viewer_output_config.h"

#include <algorithm>

namespace headless_training
{

uint32_t ClampSensorIndex(uint32_t sensorIndex, size_t sensorCount)
{
  if(sensorCount == 0)
  {
    return 0;
  }
  return std::min(sensorIndex, static_cast<uint32_t>(sensorCount - 1));
}

RendererOutputConfig BuildViewerOutputConfig(const TrainingSceneDescription& scene, const ViewerState& state)
{
  RendererOutputConfig config;
  config.tile.atlas.enabled = false;
  config.tile.requestedChannels = TileAovChannelMask::None();

  if(state.lidar.enableCompute)
  {
    config.lidar.sensors = scene.lidarSensors;
  }
  config.lidar.visualization.enabled = state.lidar.enableCompute && state.lidar.showOverlay && !scene.lidarSensors.empty();
  config.lidar.visualization.visualizeAllSensors = state.lidar.displayMode == SensorDisplayMode::All;
  config.lidar.visualization.sensorIndex = ClampSensorIndex(state.lidar.sensorIndex, scene.lidarSensors.size());
  config.lidar.visualization.pointSizePixels = std::max(state.lidar.pointSizePixels, 0.0f);

  if(state.heightScan.enableCompute)
  {
    config.heightScan.sensors = scene.heightScanSensors;
  }
  config.heightScan.visualization.enabled =
      state.heightScan.enableCompute && state.heightScan.showOverlay && !scene.heightScanSensors.empty();
  config.heightScan.visualization.visualizeAllSensors = state.heightScan.displayMode == SensorDisplayMode::All;
  config.heightScan.visualization.sensorIndex =
      ClampSensorIndex(state.heightScan.sensorIndex, scene.heightScanSensors.size());
  config.heightScan.visualization.pointSizePixels = std::max(state.heightScan.pointSizePixels, 0.0f);

  return config;
}

} // namespace headless_training
