#include "viewer_camera_controller.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

namespace headless_training
{
namespace
{
constexpr float kMinDistance = 0.05f;
constexpr float kMaxPitchRadians = glm::half_pi<float>() - 0.01f;
constexpr float kOrbitSensitivity = 0.008f;
constexpr float kPanSensitivity = 0.0015f;
constexpr float kZoomStep = 0.12f;

bool IsFinite(glm::vec3 value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback)
{
  if(!IsFinite(value) || glm::length2(value) < 1.0e-12f)
  {
    value = fallback;
  }
  if(!IsFinite(value) || glm::length2(value) < 1.0e-12f)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

glm::vec3 ForwardFromAngles(float yaw, float pitch)
{
  const float cp = std::cos(pitch);
  return glm::normalize(glm::vec3(std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp));
}

void IncludePoint(glm::vec3& minPoint, glm::vec3& maxPoint, bool& valid, glm::vec3 point)
{
  if(!IsFinite(point))
  {
    return;
  }
  minPoint = valid ? glm::min(minPoint, point) : point;
  maxPoint = valid ? glm::max(maxPoint, point) : point;
  valid = true;
}
} // namespace

void ViewerCameraController::reset(const CameraSpec& camera, glm::vec3 target)
{
  m_target = target;
  const glm::vec3 toTarget = target - camera.position;
  m_distance = std::max(glm::length(toTarget), kMinDistance);

  const glm::vec3 forward = NormalizeOr(toTarget, camera.forward);
  m_pitchRadians = std::clamp(std::asin(std::clamp(forward.y, -1.0f, 1.0f)), -kMaxPitchRadians, kMaxPitchRadians);
  m_yawRadians = std::atan2(forward.x, -forward.z);
  m_verticalFovDegrees = std::clamp(camera.verticalFovDegrees, 1.0f, 179.0f);
  m_clipStart = camera.clipStart;
  m_clipEnd = camera.clipEnd;
}

void ViewerCameraController::update(const ViewerCameraInput& input)
{
  if(input.orbit)
  {
    m_yawRadians -= input.deltaX * kOrbitSensitivity;
    m_pitchRadians = std::clamp(m_pitchRadians - input.deltaY * kOrbitSensitivity, -kMaxPitchRadians, kMaxPitchRadians);
  }

  const glm::vec3 forward = ForwardFromAngles(m_yawRadians, m_pitchRadians);
  const glm::vec3 right = NormalizeOr(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 up = NormalizeOr(glm::cross(right, forward), glm::vec3(0.0f, 1.0f, 0.0f));

  if(input.pan)
  {
    const float scale = std::max(m_distance, kMinDistance) * kPanSensitivity;
    m_target += (-right * input.deltaX + up * input.deltaY) * scale;
  }

  if(std::fabs(input.wheelDelta) > 0.0f)
  {
    m_distance *= std::pow(1.0f - kZoomStep, input.wheelDelta);
    m_distance = std::max(m_distance, kMinDistance);
  }
}

CameraSpec ViewerCameraController::camera() const
{
  CameraSpec result;
  const glm::vec3 forward = ForwardFromAngles(m_yawRadians, m_pitchRadians);
  const glm::vec3 right = NormalizeOr(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
  result.name = "viewer_orbit";
  result.position = m_target - forward * m_distance;
  result.forward = forward;
  result.up = NormalizeOr(glm::cross(right, forward), glm::vec3(0.0f, 1.0f, 0.0f));
  result.verticalFovDegrees = m_verticalFovDegrees;
  result.horizontalFovDegrees = 0.0f;
  result.clipStart = m_clipStart;
  result.clipEnd = m_clipEnd;
  return result;
}

void ViewerCameraController::setVerticalFovDegrees(float fovDegrees)
{
  m_verticalFovDegrees = std::clamp(fovDegrees, 1.0f, 179.0f);
}

void ViewerCameraController::setClipRange(float clipStart, float clipEnd)
{
  m_clipStart = std::max(clipStart, 0.0001f);
  m_clipEnd = std::max(clipEnd, m_clipStart + 0.0001f);
}

glm::vec3 ComputeViewerFocusTarget(const TrainingSceneDescription& scene, const CameraSpec& fallbackCamera)
{
  glm::vec3 minPoint{std::numeric_limits<float>::max()};
  glm::vec3 maxPoint{std::numeric_limits<float>::lowest()};
  bool valid = false;
  for(const TrainingMeshInstance& mesh : scene.meshes)
  {
    if(!mesh.visible)
    {
      continue;
    }
    for(const MeshVertex& vertex : mesh.geometry.vertices)
    {
      IncludePoint(minPoint, maxPoint, valid, glm::vec3(mesh.worldTransform * glm::vec4(vertex.pos, 1.0f)));
    }
  }
  if(valid)
  {
    return (minPoint + maxPoint) * 0.5f;
  }
  return fallbackCamera.position + NormalizeOr(fallbackCamera.forward, glm::vec3(0.0f, 0.0f, -1.0f)) * 5.0f;
}

} // namespace headless_training
