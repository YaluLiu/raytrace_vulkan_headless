#pragma once

#include "training_scene.h"

#include <optional>

#include <glm/glm.hpp>

namespace headless_training
{

struct PreviewCameraOptions
{
  std::optional<glm::vec3> position;
  std::optional<glm::vec3> target;
  std::optional<float> verticalFovDegrees;
  std::optional<float> distanceScale;
};

CameraSpec BuildPreviewCamera(const TrainingSceneDescription& scene, const PreviewCameraOptions& options = {});

} // namespace headless_training
