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
constexpr float kGravityPreviewDistanceScale = 1.15f;
constexpr float kGravityPreviewVerticalFovDegrees = 90.0f;
constexpr float kMinPreviewRadius = 0.5f;
constexpr float kDirectionEpsilon = 1.0e-6f;

struct SceneBounds
{
  glm::vec3 min{std::numeric_limits<float>::max()};
  glm::vec3 max{std::numeric_limits<float>::lowest()};
  bool valid{false};
};

struct PreviewBounds
{
  SceneBounds subject;
  SceneBounds ground;
  SceneBounds all;
};

struct PreviewDirection
{
  glm::vec3 forward{0.0f, -1.0f, 0.0f};
  bool fromSceneGravity{false};
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

PreviewBounds ComputePreviewBounds(const TrainingSceneDescription& scene)
{
  PreviewBounds bounds;
  for(const TrainingMeshInstance& mesh : scene.meshes)
  {
    if(!mesh.visible)
    {
      continue;
    }
    for(const MeshVertex& vertex : mesh.geometry.vertices)
    {
      const glm::vec4 world = mesh.worldTransform * glm::vec4(vertex.pos, 1.0f);
      const glm::vec3 point = glm::vec3(world);
      IncludePoint(bounds.all, point);
      if((mesh.traceMask & kTraceMaskGround) != 0u)
      {
        IncludePoint(bounds.ground, point);
      }
      else
      {
        IncludePoint(bounds.subject, point);
      }
    }
  }
  return bounds;
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

PreviewDirection ResolvePreviewDirection(const TrainingSceneDescription& scene)
{
  for(const HeightScanSensorSpec& sensor : scene.heightScanSensors)
  {
    if(IsFinite(sensor.params.gravityDirectionWs) &&
       glm::length2(sensor.params.gravityDirectionWs) >= kDirectionEpsilon * kDirectionEpsilon)
    {
      return {glm::normalize(sensor.params.gravityDirectionWs), true};
    }
  }
  return {};
}

float ComputeProjectedHalfExtent(const SceneBounds& bounds, glm::vec3 target, glm::vec3 forward)
{
  if(!bounds.valid)
  {
    return kMinPreviewRadius;
  }

  const glm::vec3 up = ResolveUp(forward);
  const glm::vec3 right = NormalizeOr(glm::cross(forward, up), glm::vec3(1.0f, 0.0f, 0.0f));
  float halfExtent = kMinPreviewRadius;
  for(int x = 0; x < 2; ++x)
  {
    for(int y = 0; y < 2; ++y)
    {
      for(int z = 0; z < 2; ++z)
      {
        const glm::vec3 corner(x == 0 ? bounds.min.x : bounds.max.x,
                               y == 0 ? bounds.min.y : bounds.max.y,
                               z == 0 ? bounds.min.z : bounds.max.z);
        const glm::vec3 relative = corner - target;
        halfExtent = std::max(halfExtent, std::fabs(glm::dot(relative, up)));
        halfExtent = std::max(halfExtent, std::fabs(glm::dot(relative, right)));
      }
    }
  }
  return halfExtent;
}
} // namespace

CameraSpec BuildPreviewCamera(const TrainingSceneDescription& scene, const PreviewCameraOptions& options)
{
  const CameraSpec fallback = MakeDefaultCamera();
  const PreviewBounds bounds = ComputePreviewBounds(scene);
  const SceneBounds focusBounds =
      bounds.subject.valid ? bounds.subject : (bounds.ground.valid ? bounds.ground : bounds.all);
  const PreviewDirection previewDirection = ResolvePreviewDirection(scene);
  const glm::vec3 defaultForward = previewDirection.forward;
  const float defaultVerticalFovDegrees =
      previewDirection.fromSceneGravity ? kGravityPreviewVerticalFovDegrees : fallback.verticalFovDegrees;
  const float verticalFovDegrees =
      ClampFinite(options.verticalFovDegrees.value_or(defaultVerticalFovDegrees), fallback.verticalFovDegrees, 1.0f,
                  179.0f);
  const float defaultDistanceScale =
      previewDirection.fromSceneGravity ? kGravityPreviewDistanceScale : kDefaultPreviewDistanceScale;
  const float distanceScale =
      ClampFinite(options.distanceScale.value_or(defaultDistanceScale), defaultDistanceScale, 0.01f, 1000.0f);

  glm::vec3 target = glm::vec3(0.0f, 1.0f, 0.0f);
  float radius = glm::length(fallback.position - target);
  if(focusBounds.valid)
  {
    target = (focusBounds.min + focusBounds.max) * 0.5f;
    radius = std::max(ComputeProjectedHalfExtent(focusBounds, target, defaultForward), kMinPreviewRadius);
  }

  const float halfFovRadians = glm::radians(verticalFovDegrees) * 0.5f;
  const float fitDistance = radius / std::max(std::tan(halfFovRadians), 0.01f);
  const float distance = std::max(fitDistance * distanceScale, kMinPreviewRadius);

  if(options.target)
  {
    target = *options.target;
  }

  glm::vec3 position = focusBounds.valid || options.target ? target - defaultForward * distance : fallback.position;
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
