#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct HeightScanParams
{
  float uStart = -10.0f;
  float uEnd = 10.0f;
  float uStep = 0.1f;
  float vStart = -10.0f;
  float vEnd = 10.0f;
  float vStep = 0.1f;
  glm::vec3 gravityDirectionWs{0.0f, 0.0f, -1.0f};
  float maxRange = 200.0f;
};

struct HeightScanSensorSpec
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  HeightScanParams params;
};

struct HeightScanVisualizationConfig
{
  bool enabled{false};
  uint32_t sensorIndex{0};
  float pointSizePixels{2.0f};
  bool visualizeAllSensors{false};
};

enum HeightScanSampleFlags : uint32_t
{
  HeightScanSampleFlagValid = 1u << 0,
  HeightScanSampleFlagHit = 1u << 1,
  HeightScanSampleFlagOutOfRange = 1u << 2,
};

struct HeightScanSample
{
  glm::vec3 positionWs{0.0f, 0.0f, 0.0f};
  float distanceMeters{0.0f};
  uint32_t sensorIndex{0};
  uint32_t uIndex{0};
  uint32_t vIndex{0};
  uint32_t flags{0};
};

struct HeightScanSensorGrid
{
  std::string name;
  uint32_t sensorIndex{0};
  uint32_t width{0};
  uint32_t height{0};
  std::vector<HeightScanSample> samples;
};

struct HeightScanFrame
{
  uint64_t frameId{0};
  std::vector<HeightScanSensorGrid> sensors;

  bool empty() const { return sensors.empty(); }
};
