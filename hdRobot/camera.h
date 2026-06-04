#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/usd/sdf/path.h>
#include <engine/renderer_types.hpp>
#include <glm/glm.hpp>
#include <string>

#include "backendHandles.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotSceneStore;

struct HdRobotCameraData
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

HdRobotCameraData HdRobotComputeTransformCameraData(const SdfPath& id, const GfMatrix4d& transform);
HdRobotCameraData HdRobotComputeCameraData(const HdCamera& camera);

class HdRobotCamera final : public HdCamera
{
public:
  HdRobotCamera(const SdfPath& id, HdRobotSceneStore& sceneStore, HdRobotCameraHandle handle);

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;
  void Finalize(HdRenderParam* renderParam) override;
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
  const HdRobotCameraData& GetCameraData() const;

private:
  HdRobotSceneStore&      _sceneStore;
  HdRobotCameraHandle       _handle;
  mutable HdRobotCameraData _cameraData;
};

PXR_NAMESPACE_CLOSE_SCOPE
