#pragma once

#include <raster/lidar_types.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct RasterLidarBasis
{
  glm::vec3 origin{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 right{1.0f, 0.0f, 0.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
};

struct RasterLidarSensorMetadata
{
  std::string name;
  uint32_t sensorIndex{0};
  uint64_t pointOffset{0};
  uint64_t pointCount{0};
  uint32_t azimuthSampleCount{0};
  uint32_t verticalSampleCount{0};
  float maxDistance{0.0f};
};

struct RasterLidarScanLayout
{
  std::vector<RasterLidarSensorMetadata> sensors;
  uint64_t totalPointCount{0};

  bool empty() const { return sensors.empty() || totalPointCount == 0; }
};

uint32_t ComputeLidarSampleCount(float startDeg, float endDeg, float stepDeg);
float ComputeLidarSampleAngleDeg(float startDeg, float endDeg, float stepDeg, uint32_t index);
RasterLidarScanLayout BuildLidarScanLayout(std::span<const RasterLidarSensorSpec> sensors);
RasterLidarBasis BuildLidarBasis(const RasterLidarSensorSpec& sensor);
glm::vec3 ComputeLidarBeamDirectionWs(const RasterLidarSensorSpec& sensor, uint32_t ringIndex, uint32_t beamIndex);
