#include "scene/camera_projection.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void RequireNear(const char* label, float actual, float expected, float tolerance = 1.0e-4f)
{
  if(std::fabs(actual - expected) <= tolerance)
  {
    return;
  }
  std::cerr << label << " expected " << expected << " but got " << actual << '\n';
  std::exit(1);
}
} // namespace

int main()
{
  CameraSpec squareFilmback;
  squareFilmback.verticalFovDegrees = 90.0f;
  squareFilmback.horizontalFovDegrees = 90.0f;
  squareFilmback.clipStart = 0.01f;
  squareFilmback.clipEnd = 100.0f;

  const VkExtent2D wideTarget{1920, 1080};

  squareFilmback.conformPolicy = CameraConformPolicy::MatchHorizontally;
  glm::mat4 projection = BuildCameraProjection(squareFilmback, wideTarget);
  RequireNear("match-horizontal x scale", projection[0][0], 1.0f);
  RequireNear("match-horizontal y scale", projection[1][1], 16.0f / 9.0f);
  RequireNear("match-horizontal vertical fov", ComputeVerticalFovForRenderTarget(squareFilmback, wideTarget),
              58.7155f);

  squareFilmback.conformPolicy = CameraConformPolicy::MatchVertically;
  projection = BuildCameraProjection(squareFilmback, wideTarget);
  RequireNear("match-vertical x scale", projection[0][0], 9.0f / 16.0f);
  RequireNear("match-vertical y scale", projection[1][1], 1.0f);

  CameraSpec legacyVerticalOnly;
  legacyVerticalOnly.verticalFovDegrees = 90.0f;
  projection = BuildCameraProjection(legacyVerticalOnly, wideTarget);
  RequireNear("legacy x scale", projection[0][0], 9.0f / 16.0f);
  RequireNear("legacy y scale", projection[1][1], 1.0f);

  return 0;
}
