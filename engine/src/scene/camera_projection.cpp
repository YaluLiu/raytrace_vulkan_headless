#include "scene/camera_projection.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr float kDefaultFovDeg = 45.0f;
constexpr float kMinFovDeg = 1.0f;
constexpr float kMaxFovDeg = 179.0f;

float clampFov(float fovDeg, float fallback = kDefaultFovDeg)
{
  if(!std::isfinite(fovDeg))
  {
    return fallback;
  }
  return std::clamp(fovDeg, kMinFovDeg, kMaxFovDeg);
}

float safeAspect(VkExtent2D renderSize)
{
  if(renderSize.width == 0 || renderSize.height == 0)
  {
    return 1.0f;
  }
  return renderSize.width / static_cast<float>(renderSize.height);
}

bool hasHorizontalFov(const CameraSpec& camera)
{
  return std::isfinite(camera.hfov_deg) && camera.hfov_deg > 0.0f;
}

CameraConformPolicy resolveFitOrCrop(CameraConformPolicy policy, float sourceAspect, float targetAspect)
{
  if(policy == CameraConformPolicy::MatchVertically || policy == CameraConformPolicy::MatchHorizontally)
  {
    return policy;
  }
  if(policy == CameraConformPolicy::DontConform)
  {
    return policy;
  }

  const bool sourceWider = sourceAspect > targetAspect;
  const bool fit = policy == CameraConformPolicy::Fit;
  return (fit != sourceWider) ? CameraConformPolicy::MatchVertically : CameraConformPolicy::MatchHorizontally;
}

glm::vec2 computeTanHalfWindow(const CameraSpec& camera, float targetAspect)
{
  const float verticalFov = glm::radians(clampFov(camera.vfov_deg));
  glm::vec2 tanHalfWindow{std::tan(verticalFov * 0.5f) * targetAspect, std::tan(verticalFov * 0.5f)};
  if(!hasHorizontalFov(camera))
  {
    return tanHalfWindow;
  }

  const float horizontalFov = glm::radians(clampFov(camera.hfov_deg, camera.vfov_deg));
  tanHalfWindow.x = std::tan(horizontalFov * 0.5f);
  tanHalfWindow.y = std::tan(verticalFov * 0.5f);
  if(tanHalfWindow.x <= 0.0f || tanHalfWindow.y <= 0.0f)
  {
    return {std::tan(verticalFov * 0.5f) * targetAspect, std::tan(verticalFov * 0.5f)};
  }

  const float sourceAspect = tanHalfWindow.x / tanHalfWindow.y;
  switch(resolveFitOrCrop(camera.conformPolicy, sourceAspect, targetAspect))
  {
    case CameraConformPolicy::MatchHorizontally:
      tanHalfWindow.y = tanHalfWindow.x / targetAspect;
      break;
    case CameraConformPolicy::MatchVertically:
      tanHalfWindow.x = tanHalfWindow.y * targetAspect;
      break;
    case CameraConformPolicy::DontConform:
      break;
    case CameraConformPolicy::Fit:
    case CameraConformPolicy::Crop:
      break;
  }

  return tanHalfWindow;
}
} // namespace

float ComputeVerticalFovForRenderTarget(const CameraSpec& camera, VkExtent2D renderSize)
{
  const float targetAspect = safeAspect(renderSize);
  const glm::vec2 tanHalfWindow = computeTanHalfWindow(camera, targetAspect);
  return clampFov(glm::degrees(2.0f * std::atan(tanHalfWindow.y)));
}

glm::mat4 BuildCameraProjection(const CameraSpec& camera, VkExtent2D renderSize)
{
  const float targetAspect = safeAspect(renderSize);
  const glm::vec2 tanHalfWindow = computeTanHalfWindow(camera, targetAspect);
  const float nearPlane = std::max(camera.clipStart, 0.001f);
  const float farPlane = std::max(camera.clipEnd, nearPlane + 0.001f);

  const float right = tanHalfWindow.x * nearPlane;
  const float top = tanHalfWindow.y * nearPlane;
  return glm::frustumRH_ZO(-right, right, -top, top, nearPlane, farPlane);
}
