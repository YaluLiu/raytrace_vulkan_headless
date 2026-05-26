#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct RasterHeightScanParams
{
  float uStart = -10.0f;
  float uEnd = 10.0f;
  float uStep = 0.1f;
  float vStart = -10.0f;
  float vEnd = 10.0f;
  float vStep = 0.1f;
  glm::vec3 rayDirection{0.0f, 0.0f, -1.0f};
  float maxRange = 200.0f;
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
  uint32_t uIndex{0};
  uint32_t vIndex{0};
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
