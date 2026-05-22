#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct RasterHeightScanParams
{
  float minX = -10.0f;
  float maxX = 10.0f;
  float stepX = 0.1f;
  float minZ = -10.0f;
  float maxZ = 10.0f;
  float stepZ = 0.1f;
  glm::vec3 rayDirection{0.0f, 0.0f, -1.0f};
  float maxDistance = 200.0f;
};

struct RasterHeightScanSensorSpec
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  RasterHeightScanParams params;
};

struct RasterHeightScanVisualizationConfig
{
  bool enabled{false};
  uint32_t sensorIndex{0};
  float pointSizePixels{2.0f};
};

enum RasterHeightScanSampleFlags : uint32_t
{
  RasterHeightScanSampleFlagValid = 1u << 0,
  RasterHeightScanSampleFlagHit = 1u << 1,
  RasterHeightScanSampleFlagOutOfRange = 1u << 2,
};

struct RasterHeightScanSample
{
  glm::vec3 positionWs{0.0f, 0.0f, 0.0f};
  float distanceMeters{0.0f};
  uint32_t sensorIndex{0};
  uint32_t xIndex{0};
  uint32_t zIndex{0};
  uint32_t flags{0};
};

struct RasterHeightScanSensorGrid
{
  std::string name;
  uint32_t sensorIndex{0};
  uint32_t width{0};
  uint32_t height{0};
  std::vector<RasterHeightScanSample> samples;
};

struct RasterHeightScanFrame
{
  uint64_t frameId{0};
  std::vector<RasterHeightScanSensorGrid> sensors;

  bool empty() const { return sensors.empty(); }
};
