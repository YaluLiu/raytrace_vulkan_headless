#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct RasterLidarParams
{
  float azimuthStartDeg = -90.0f;
  float azimuthEndDeg = 90.0f;
  float azimuthStepDeg = 0.5f;
  float verticalStartDeg = -2.0f;
  float verticalEndDeg = -20.0f;
  float verticalStepDeg = 1.0f;
  float maxRange = 200.0f;
  float intensity = 1.0f;
};

struct RasterLidarSensorSpec
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  RasterLidarParams params;
};

struct RasterLidarVisualizationConfig
{
  bool enabled{false};
  uint32_t sensorIndex{0};
  float pointSizePixels{2.0f};
};

enum RasterLidarPointFlags : uint32_t
{
  RasterLidarPointFlagValid = 1u << 0,
  RasterLidarPointFlagHit = 1u << 1,
  RasterLidarPointFlagOutOfRange = 1u << 2,
};

struct RasterLidarPoint
{
  glm::vec3 positionWs{0.0f, 0.0f, 0.0f};
  float rangeMeters{0.0f};
  uint32_t sensorIndex{0};
  uint32_t ringIndex{0};
  uint32_t beamIndex{0};
  uint32_t flags{0};
  float intensity{0.0f};
};

struct RasterLidarSensorPointCloud
{
  std::string name;
  uint32_t sensorIndex{0};
  uint32_t width{0};
  uint32_t height{0};
  std::vector<RasterLidarPoint> points;
};

struct RasterLidarFramePointCloud
{
  uint64_t frameId{0};
  std::vector<RasterLidarSensorPointCloud> sensors;

  bool empty() const { return sensors.empty(); }
};
