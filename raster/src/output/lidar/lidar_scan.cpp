#include "output/lidar/lidar_scan.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>

namespace
{
constexpr float kLidarParamEpsilon = 1.0e-4f;
constexpr float kLidarBasisEpsilon = 1.0e-6f;

float sanitizeStep(float stepDeg)
{
  if(!std::isfinite(stepDeg))
  {
    return kLidarParamEpsilon;
  }
  return std::max(std::fabs(stepDeg), kLidarParamEpsilon);
}

float sanitizeMaxRange(float maxRange)
{
  if(!std::isfinite(maxRange))
  {
    return kLidarParamEpsilon;
  }
  return std::max(maxRange, kLidarParamEpsilon);
}

glm::vec3 safeNormalize(glm::vec3 value, glm::vec3 fallback)
{
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
     glm::length(value) < kLidarBasisEpsilon)
  {
    value = fallback;
  }
  if(glm::length(value) < kLidarBasisEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}
} // namespace

uint32_t ComputeLidarSampleCount(float startDeg, float endDeg, float stepDeg)
{
  if(!std::isfinite(startDeg) || !std::isfinite(endDeg))
  {
    return 1;
  }

  const double span = std::fabs(static_cast<double>(endDeg) - static_cast<double>(startDeg));
  const double step = static_cast<double>(sanitizeStep(stepDeg));
  const double count = std::floor(span / step + 0.5) + 1.0;
  if(!std::isfinite(count) || count <= 1.0)
  {
    return 1;
  }
  if(count > static_cast<double>(std::numeric_limits<uint32_t>::max()))
  {
    return std::numeric_limits<uint32_t>::max();
  }
  return static_cast<uint32_t>(count);
}

float ComputeLidarSampleAngleDeg(float startDeg, float endDeg, float stepDeg, uint32_t index)
{
  if(!std::isfinite(startDeg) || !std::isfinite(endDeg))
  {
    return 0.0f;
  }
  const float direction = endDeg >= startDeg ? 1.0f : -1.0f;
  const float angle = startDeg + direction * sanitizeStep(stepDeg) * static_cast<float>(index);
  return direction > 0.0f ? std::min(angle, endDeg) : std::max(angle, endDeg);
}

RasterLidarScanLayout BuildLidarScanLayout(std::span<const RasterLidarSensorSpec> sensors)
{
  RasterLidarScanLayout layout;
  layout.sensors.reserve(sensors.size());

  uint64_t nextOffset = 0;
  for(size_t sensorIndex = 0; sensorIndex < sensors.size(); ++sensorIndex)
  {
    const RasterLidarSensorSpec& sensor = sensors[sensorIndex];
    const uint32_t azimuthCount =
        ComputeLidarSampleCount(sensor.params.azimuthStartDeg, sensor.params.azimuthEndDeg, sensor.params.azimuthStepDeg);
    const uint32_t verticalCount =
        ComputeLidarSampleCount(sensor.params.verticalStartDeg, sensor.params.verticalEndDeg, sensor.params.verticalStepDeg);
    const uint64_t pointCount = static_cast<uint64_t>(azimuthCount) * static_cast<uint64_t>(verticalCount);

    RasterLidarSensorMetadata metadata;
    metadata.name = sensor.name;
    metadata.sensorIndex = static_cast<uint32_t>(sensorIndex);
    metadata.pointOffset = nextOffset;
    metadata.pointCount = pointCount;
    metadata.azimuthSampleCount = azimuthCount;
    metadata.verticalSampleCount = verticalCount;
    metadata.maxRange = sanitizeMaxRange(sensor.params.maxRange);
    layout.sensors.push_back(std::move(metadata));

    if(std::numeric_limits<uint64_t>::max() - nextOffset < pointCount)
    {
      layout.sensors.clear();
      layout.totalPointCount = 0;
      return layout;
    }
    nextOffset += pointCount;
  }

  layout.totalPointCount = nextOffset;
  return layout;
}

RasterLidarBasis BuildLidarBasis(const RasterLidarSensorSpec& sensor)
{
  RasterLidarBasis basis;
  basis.origin = sensor.position;
  basis.forward = safeNormalize(sensor.forward, glm::vec3(0.0f, 0.0f, -1.0f));
  basis.up = safeNormalize(sensor.up, glm::vec3(0.0f, 1.0f, 0.0f));

  if(glm::length(glm::cross(basis.forward, basis.up)) < kLidarBasisEpsilon)
  {
    basis.up = std::fabs(glm::dot(basis.forward, glm::vec3(0.0f, 1.0f, 0.0f))) < 0.95f ?
                   glm::vec3(0.0f, 1.0f, 0.0f) :
                   glm::vec3(0.0f, 0.0f, 1.0f);
  }

  basis.right = safeNormalize(glm::cross(basis.forward, basis.up), glm::vec3(1.0f, 0.0f, 0.0f));
  basis.up = safeNormalize(glm::cross(basis.right, basis.forward), glm::vec3(0.0f, 1.0f, 0.0f));
  return basis;
}

glm::vec3 ComputeLidarBeamDirectionWs(const RasterLidarSensorSpec& sensor, uint32_t ringIndex, uint32_t beamIndex)
{
  const RasterLidarBasis basis = BuildLidarBasis(sensor);
  const float azimuthDeg = ComputeLidarSampleAngleDeg(sensor.params.azimuthStartDeg, sensor.params.azimuthEndDeg,
                                                      sensor.params.azimuthStepDeg, beamIndex);
  const float verticalDeg = ComputeLidarSampleAngleDeg(sensor.params.verticalStartDeg, sensor.params.verticalEndDeg,
                                                       sensor.params.verticalStepDeg, ringIndex);
  const float azimuthRad = glm::radians(azimuthDeg);
  const float verticalRad = glm::radians(verticalDeg);

  const float cosVertical = std::cos(verticalRad);
  glm::vec3 direction = basis.forward * (std::cos(azimuthRad) * cosVertical) +
                        basis.right * (std::sin(azimuthRad) * cosVertical) + basis.up * std::sin(verticalRad);
  return safeNormalize(direction, basis.forward);
}
