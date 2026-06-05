#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/usd/sdf/path.h>
#include <engine/renderer_types.hpp>
#include <glm/glm.hpp>
#include <string>

#include "backendHandles.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

class SceneStore;

struct CameraData
{
  std::string name;
  glm::vec3   position  = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3   forward   = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3   up        = glm::vec3(0.0f, 1.0f, 0.0f);
  float       vfov_deg  = 45.0f;
  float       hfov_deg  = 0.0f;
  float       clipStart = 0.1f;
  float       clipEnd   = 1000.0f;
  CameraConformPolicy conformPolicy = CameraConformPolicy::MatchVertically;
};

} // namespace hdrobot

hdrobot::CameraData HdRobotComputeTransformCameraData(const SdfPath& id, const GfMatrix4d& transform);
hdrobot::CameraData HdRobotComputeCameraData(const HdCamera& camera);

class HdRobotCamera final : public HdCamera
{
public:
  HdRobotCamera(const SdfPath& id, hdrobot::SceneStore& sceneStore, hdrobot::CameraHandle handle);

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;
  void Finalize(HdRenderParam* renderParam) override;
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
  const hdrobot::CameraData& GetCameraData() const;

private:
  hdrobot::SceneStore& _sceneStore;
  hdrobot::CameraHandle _handle;
  mutable hdrobot::CameraData _cameraData;
};

PXR_NAMESPACE_CLOSE_SCOPE
