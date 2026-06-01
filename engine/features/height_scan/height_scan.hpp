#pragma once

#include <engine/height_scan_types.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct HeightScanBasis
{
  glm::vec3 origin{0.0f, 0.0f, 0.0f};
  glm::vec3 gravityDirectionWs{0.0f, 0.0f, -1.0f};
  glm::vec3 axisU{1.0f, 0.0f, 0.0f};
  glm::vec3 axisV{0.0f, 1.0f, 0.0f};
};

struct HeightScanSensorMetadata
{
  std::string name;
  uint32_t sensorIndex{0};
  uint64_t sampleOffset{0};
  uint64_t sampleCount{0};
  uint32_t uSampleCount{0};
  uint32_t vSampleCount{0};
  float maxRange{0.0f};
};

struct HeightScanLayout
{
  std::vector<HeightScanSensorMetadata> sensors;
  uint64_t totalSampleCount{0};

  bool empty() const { return sensors.empty() || totalSampleCount == 0; }
};

uint32_t ComputeHeightScanSampleCount(float start, float end, float step);
float ComputeHeightScanSampleOffset(float start, float end, float step, uint32_t index);
HeightScanLayout BuildHeightScanLayout(std::span<const HeightScanSensorSpec> sensors);
HeightScanBasis BuildHeightScanBasis(const HeightScanSensorSpec& sensor);
glm::vec3 ComputeHeightScanOriginWs(const HeightScanSensorSpec& sensor, uint32_t uIndex, uint32_t vIndex);
