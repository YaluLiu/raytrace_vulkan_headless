#pragma once

#include <pxr/imaging/hd/renderDelegate.h>

#include <cstdint>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

struct TileConfig
{
  bool     enabled = false;
  bool     colorEnabled = true;
  bool     depthEnabled = true;
  uint32_t cameraWidth = 1920;
  uint32_t cameraHeight = 1080;
  uint32_t gridColumns = 10;
  uint32_t gridRows = 10;
};

struct LidarVisualizationConfig
{
  bool enabled = false;
  uint32_t sensorIndex = 0;
  float pointSizePixels = 2.0f;
  bool visualizeAllSensors = false;
};

struct HeightScanVisualizationConfig
{
  bool enabled = false;
  uint32_t sensorIndex = 0;
  float pointSizePixels = 2.0f;
  bool visualizeAllSensors = false;
};

class RenderParam final : public HdRenderParam
{
 public:
  void SetTileConfig(const TileConfig& config);
  TileConfig GetTileConfig() const;
  void SetLidarVisualizationConfig(const LidarVisualizationConfig& config);
  LidarVisualizationConfig GetLidarVisualizationConfig() const;
  void SetHeightScanVisualizationConfig(const HeightScanVisualizationConfig& config);
  HeightScanVisualizationConfig GetHeightScanVisualizationConfig() const;

private:
  mutable std::mutex mutex;
  TileConfig tileConfig;
  LidarVisualizationConfig lidarVisualizationConfig;
  HeightScanVisualizationConfig heightScanVisualizationConfig;

};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
