#include "output/height_scan/height_scan.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
constexpr float kHeightScanParamEpsilon = 1.0e-4f;
constexpr float kHeightScanBasisEpsilon = 1.0e-6f;

float sanitizeStep(float step)
{
  if(!std::isfinite(step))
  {
    return kHeightScanParamEpsilon;
  }
  return std::max(std::fabs(step), kHeightScanParamEpsilon);
}

float sanitizeMaxDistance(float maxDistance)
{
  if(!std::isfinite(maxDistance))
  {
    return kHeightScanParamEpsilon;
  }
  return std::max(maxDistance, kHeightScanParamEpsilon);
}

glm::vec3 safeNormalize(glm::vec3 value, glm::vec3 fallback)
{
  const float length = glm::length(value);
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
     !std::isfinite(length) || length < kHeightScanBasisEpsilon)
  {
    value = fallback;
  }
  const float fallbackLength = glm::length(value);
  if(!std::isfinite(fallbackLength) || fallbackLength < kHeightScanBasisEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}
} // namespace

uint32_t ComputeHeightScanSampleCount(float start, float end, float step)
{
  if(!std::isfinite(start) || !std::isfinite(end))
  {
    return 1;
  }

  const double span = std::fabs(static_cast<double>(end) - static_cast<double>(start));
  const double sampleStep = static_cast<double>(sanitizeStep(step));
  const double count = std::floor(span / sampleStep + 0.5) + 1.0;
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

float ComputeHeightScanSampleOffset(float start, float end, float step, uint32_t index)
{
  if(!std::isfinite(start) || !std::isfinite(end))
  {
    return 0.0f;
  }
  const float direction = end >= start ? 1.0f : -1.0f;
  const float offset = start + direction * sanitizeStep(step) * static_cast<float>(index);
  return direction > 0.0f ? std::min(offset, end) : std::max(offset, end);
}

RasterHeightScanLayout BuildHeightScanLayout(std::span<const RasterHeightScanSensorSpec> sensors)
{
  RasterHeightScanLayout layout;
  layout.sensors.reserve(sensors.size());

  uint64_t nextOffset = 0;
  for(size_t sensorIndex = 0; sensorIndex < sensors.size(); ++sensorIndex)
  {
    const RasterHeightScanSensorSpec& sensor = sensors[sensorIndex];
    const uint32_t xCount = ComputeHeightScanSampleCount(sensor.params.minX, sensor.params.maxX, sensor.params.stepX);
    const uint32_t zCount = ComputeHeightScanSampleCount(sensor.params.minZ, sensor.params.maxZ, sensor.params.stepZ);
    const uint64_t sampleCount = static_cast<uint64_t>(xCount) * static_cast<uint64_t>(zCount);

    RasterHeightScanSensorMetadata metadata;
    metadata.name = sensor.name;
    metadata.sensorIndex = static_cast<uint32_t>(sensorIndex);
    metadata.sampleOffset = nextOffset;
    metadata.sampleCount = sampleCount;
    metadata.xSampleCount = xCount;
    metadata.zSampleCount = zCount;
    metadata.maxDistance = sanitizeMaxDistance(sensor.params.maxDistance);
    layout.sensors.push_back(std::move(metadata));

    if(std::numeric_limits<uint64_t>::max() - nextOffset < sampleCount)
    {
      layout.sensors.clear();
      layout.totalSampleCount = 0;
      return layout;
    }
    nextOffset += sampleCount;
  }

  layout.totalSampleCount = nextOffset;
  return layout;
}

RasterHeightScanBasis BuildHeightScanBasis(const RasterHeightScanSensorSpec& sensor)
{
  RasterHeightScanBasis basis;
  basis.origin = sensor.position;
  basis.rayDirection = safeNormalize(sensor.params.rayDirection, glm::vec3(0.0f, 0.0f, -1.0f));

  const glm::vec3 referenceAxis =
      std::fabs(glm::dot(basis.rayDirection, glm::vec3(0.0f, 1.0f, 0.0f))) < 0.95f ?
          glm::vec3(0.0f, 1.0f, 0.0f) :
          glm::vec3(1.0f, 0.0f, 0.0f);
  basis.axisX = safeNormalize(glm::cross(basis.rayDirection, referenceAxis), glm::vec3(1.0f, 0.0f, 0.0f));
  basis.axisZ = safeNormalize(glm::cross(basis.axisX, basis.rayDirection), glm::vec3(0.0f, 1.0f, 0.0f));
  return basis;
}

glm::vec3 ComputeHeightScanOriginWs(const RasterHeightScanSensorSpec& sensor, uint32_t xIndex, uint32_t zIndex)
{
  const RasterHeightScanBasis basis = BuildHeightScanBasis(sensor);
  const float xOffset =
      ComputeHeightScanSampleOffset(sensor.params.minX, sensor.params.maxX, sensor.params.stepX, xIndex);
  const float zOffset =
      ComputeHeightScanSampleOffset(sensor.params.minZ, sensor.params.maxZ, sensor.params.stepZ, zIndex);
  return basis.origin + basis.axisX * xOffset + basis.axisZ * zOffset;
}
