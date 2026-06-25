#pragma once

#include "training_scene.h"

#include <glm/glm.hpp>

namespace headless_training
{

struct ViewerCameraInput
{
  bool orbit{false};
  bool pan{false};
  float deltaX{0.0f};
  float deltaY{0.0f};
  float wheelDelta{0.0f};
  int viewportWidth{1};
  int viewportHeight{1};
};

class ViewerCameraController
{
public:
  void reset(const CameraSpec& camera, glm::vec3 target);
  void update(const ViewerCameraInput& input);

  CameraSpec camera() const;
  glm::vec3 target() const { return m_target; }
  void setVerticalFovDegrees(float fovDegrees);
  void setClipRange(float clipStart, float clipEnd);

private:
  glm::vec3 m_target{0.0f};
  glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
  glm::vec3 m_up{0.0f, 1.0f, 0.0f};
  float m_distance{5.0f};
  float m_yawRadians{0.0f};
  float m_pitchRadians{0.0f};
  float m_verticalFovDegrees{45.0f};
  float m_horizontalFovDegrees{0.0f};
  float m_clipStart{0.1f};
  float m_clipEnd{1000.0f};
  CameraConformPolicy m_conformPolicy{CameraConformPolicy::MatchVertically};
};

glm::vec3 ComputeViewerFocusTarget(const TrainingSceneDescription& scene, const CameraSpec& fallbackCamera);

} // namespace headless_training
