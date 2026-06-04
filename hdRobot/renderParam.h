#pragma once

#include <pxr/imaging/hd/renderDelegate.h>

#include <cstdint>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotTileConfig
{
  bool     enabled = false;
  bool     colorEnabled = true;
  bool     depthEnabled = true;
  uint32_t cameraWidth = 1920;
  uint32_t cameraHeight = 1080;
  uint32_t gridColumns = 10;
  uint32_t gridRows = 10;
};

struct HdRobotLidarVisualizationConfig
{
  bool enabled = false;
  uint32_t sensorIndex = 0;
  float pointSizePixels = 2.0f;
  bool visualizeAllSensors = false;
};

struct HdRobotHeightScanVisualizationConfig
{
  bool enabled = false;
  uint32_t sensorIndex = 0;
  float pointSizePixels = 2.0f;
  bool visualizeAllSensors = false;
};

class HdRobotRenderParam final : public HdRenderParam
{
 public:
  void SetTileConfig(const HdRobotTileConfig& config);
  HdRobotTileConfig GetTileConfig() const;
  void SetLidarVisualizationConfig(const HdRobotLidarVisualizationConfig& config);
  HdRobotLidarVisualizationConfig GetLidarVisualizationConfig() const;
  void SetHeightScanVisualizationConfig(const HdRobotHeightScanVisualizationConfig& config);
  HdRobotHeightScanVisualizationConfig GetHeightScanVisualizationConfig() const;

private:
  mutable std::mutex mutex;
  HdRobotTileConfig tileConfig;
  HdRobotLidarVisualizationConfig lidarVisualizationConfig;
  HdRobotHeightScanVisualizationConfig heightScanVisualizationConfig;

};

PXR_NAMESPACE_CLOSE_SCOPE
