#include "preview_camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

namespace headless_training
{
namespace
{
constexpr float kDefaultPreviewDistanceScale = 1.35f;
constexpr float kMinPreviewRadius = 0.5f;
constexpr float kDirectionEpsilon = 1.0e-6f;

struct SceneBounds
{
  glm::vec3 min{std::numeric_limits<float>::max()};
  glm::vec3 max{std::numeric_limits<float>::lowest()};
  bool valid{false};
};

bool IsFinite(glm::vec3 value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback)
{
  if(!IsFinite(value) || glm::length2(value) < kDirectionEpsilon * kDirectionEpsilon)
  {
    value = fallback;
  }
  if(!IsFinite(value) || glm::length2(value) < kDirectionEpsilon * kDirectionEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
}

float ClampFinite(float value, float fallback, float minValue, float maxValue)
{
  if(!std::isfinite(value))
  {
    value = fallback;
  }
  return std::clamp(value, minValue, maxValue);
}

void IncludePoint(SceneBounds& bounds, glm::vec3 point)
{
  if(!IsFinite(point))
  {
    return;
  }
  bounds.min = bounds.valid ? glm::min(bounds.min, point) : point;
  bounds.max = bounds.valid ? glm::max(bounds.max, point) : point;
  bounds.valid = true;
}

SceneBounds ComputeSceneBounds(const TrainingSceneDescription& scene)
{
  SceneBounds bounds;
  for(const TrainingMeshInstance& mesh : scene.meshes)
  {
    if(!mesh.visible)
    {
      continue;
    }
    for(const MeshVertex& vertex : mesh.geometry.vertices)
    {
      const glm::vec4 world = mesh.worldTransform * glm::vec4(vertex.pos, 1.0f);
      IncludePoint(bounds, glm::vec3(world));
    }
  }
  return bounds;
}

glm::vec3 DefaultPreviewForward()
{
  return NormalizeOr(glm::vec3(-5.0f, -3.0f, 4.0f), glm::vec3(0.0f, -0.5f, 1.0f));
}

glm::vec3 ResolveUp(glm::vec3 forward)
{
  glm::vec3 up(0.0f, 1.0f, 0.0f);
  if(glm::length2(glm::cross(forward, up)) < kDirectionEpsilon * kDirectionEpsilon)
  {
    up = glm::vec3(0.0f, 0.0f, 1.0f);
  }
  return up;
}
} // namespace

CameraSpec BuildPreviewCamera(const TrainingSceneDescription& scene, const PreviewCameraOptions& options)
{
  const CameraSpec fallback = MakeDefaultCamera();
  const SceneBounds bounds = ComputeSceneBounds(scene);
  const glm::vec3 defaultForward = DefaultPreviewForward();
  const float verticalFovDegrees =
      ClampFinite(options.verticalFovDegrees.value_or(fallback.verticalFovDegrees), fallback.verticalFovDegrees, 1.0f,
                  179.0f);
  const float distanceScale =
      ClampFinite(options.distanceScale.value_or(kDefaultPreviewDistanceScale), kDefaultPreviewDistanceScale, 0.01f,
                  1000.0f);

  glm::vec3 target = glm::vec3(0.0f, 1.0f, 0.0f);
  float radius = glm::length(fallback.position - target);
  if(bounds.valid)
  {
    target = (bounds.min + bounds.max) * 0.5f;
    radius = std::max(glm::length(bounds.max - target), kMinPreviewRadius);
  }

  const float halfFovRadians = glm::radians(verticalFovDegrees) * 0.5f;
  const float fitDistance = radius / std::max(std::sin(halfFovRadians), 0.01f);
  const float distance = std::max(fitDistance * distanceScale, kMinPreviewRadius);

  if(options.target)
  {
    target = *options.target;
  }

  glm::vec3 position = bounds.valid || options.target ? target - defaultForward * distance : fallback.position;
  if(options.position)
  {
    position = *options.position;
  }

  CameraSpec camera;
  camera.name = "headless_preview_auto";
  camera.position = position;
  camera.forward = NormalizeOr(target - position, fallback.forward);
  camera.up = ResolveUp(camera.forward);
  camera.verticalFovDegrees = verticalFovDegrees;
  camera.horizontalFovDegrees = 0.0f;
  camera.clipStart = fallback.clipStart;
  camera.clipEnd = std::max(fallback.clipEnd, distance + radius * 4.0f);
  camera.conformPolicy = fallback.conformPolicy;
  return camera;
}

} // namespace headless_training
