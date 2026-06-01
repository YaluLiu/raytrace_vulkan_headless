#include "features/height_scan/height_scan.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
bool NearlyEqual(const glm::vec3& lhs, const glm::vec3& rhs, float tolerance = 1.0e-5f)
{
  return glm::length(lhs - rhs) <= tolerance;
}

void RequireVec3(const char* label, const glm::vec3& actual, const glm::vec3& expected)
{
  if(NearlyEqual(actual, expected))
  {
    return;
  }
  std::cerr << label << " expected (" << expected.x << ", " << expected.y << ", " << expected.z << ") but got ("
            << actual.x << ", " << actual.y << ", " << actual.z << ")\n";
  std::exit(1);
}
} // namespace

int main()
{
  {
    RasterHeightScanSensorSpec sensor;
    sensor.position = glm::vec3(3.0f, 4.0f, 5.0f);
    sensor.forward = glm::vec3(0.0f, 1.0f, 0.0f);
    sensor.up = glm::vec3(0.0f, 0.0f, 1.0f);
    sensor.params.gravityDirectionWs = glm::vec3(0.0f, 0.0f, -1.0f);

    const RasterHeightScanBasis basis = BuildHeightScanBasis(sensor);
    RequireVec3("origin", basis.origin, sensor.position);
    RequireVec3("axisU", basis.axisU, glm::vec3(1.0f, 0.0f, 0.0f));
    RequireVec3("axisV", basis.axisV, glm::vec3(0.0f, 1.0f, 0.0f));

    const glm::vec3 sample = ComputeHeightScanOriginWs(sensor, 0, 1);
    RequireVec3("sample", sample, sensor.position + basis.axisU * sensor.params.uStart +
                                      basis.axisV * (sensor.params.vStart + sensor.params.vStep));
  }

  {
    RasterHeightScanSensorSpec sensor;
    sensor.forward = glm::vec3(1.0f, 0.0f, -1.0f);
    sensor.up = glm::vec3(0.0f, 0.0f, 1.0f);
    sensor.params.gravityDirectionWs = glm::vec3(0.0f, 0.0f, -1.0f);

    const RasterHeightScanBasis basis = BuildHeightScanBasis(sensor);
    RequireVec3("projected axisV", basis.axisV, glm::vec3(1.0f, 0.0f, 0.0f));
    RequireVec3("projected axisU", basis.axisU, glm::vec3(0.0f, -1.0f, 0.0f));
  }

  {
    RasterHeightScanSensorSpec sensor;
    sensor.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    sensor.up = glm::vec3(0.0f, 1.0f, 0.0f);
    sensor.params.gravityDirectionWs = glm::vec3(0.0f, 0.0f, -1.0f);

    const RasterHeightScanBasis basis = BuildHeightScanBasis(sensor);
    RequireVec3("fallback axisU", basis.axisU, glm::vec3(1.0f, 0.0f, 0.0f));
    RequireVec3("fallback axisV", basis.axisV, glm::vec3(0.0f, 1.0f, 0.0f));
  }
  return 0;
}
