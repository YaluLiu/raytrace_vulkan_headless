#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Convert a Vulkan [0,1] perspective depth value to positive view-axis depth.
// The result uses the same scene units as the camera clip range. Non-finite
// values map to the far clip, matching the tile background convention.
inline float LinearizePerspectiveDepth(float normalizedDepth, float clipStart, float clipEnd)
{
  const float nearPlane = std::max(clipStart, 0.001f);
  const float farPlane = std::max(clipEnd, nearPlane + 0.001f);
  if(!std::isfinite(normalizedDepth))
  {
    return farPlane;
  }

  const float depth = std::clamp(normalizedDepth, 0.0f, 1.0f);
  return nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
}

// CPU readback of the cameras rendered by the tile-depth output.
//
// Values are tightly packed in camera-major [camera][y][x] order. Rows use
// the conventional image orientation (y=0 is the top row). Each value is the
// Vulkan zero-to-one perspective depth written by the tile shaders; 1.0 is
// the clear/background value.
struct TileDepthFrame
{
  uint64_t frameId{0};
  uint32_t cameraCount{0};
  uint32_t width{0};
  uint32_t height{0};
  std::vector<float> values;
};
