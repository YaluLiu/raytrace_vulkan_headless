#pragma once

#include <pxr/imaging/hd/camera.h>
#include <glm/glm.hpp>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam;

struct HdRobotCameraData
{
  std::string name;
  glm::vec3   position  = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3   forward   = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3   up        = glm::vec3(0.0f, 1.0f, 0.0f);
  float       vfov_deg  = 45.0f;
  float       clipStart = 0.1f;
  float       clipEnd   = 1000.0f;
};

HdRobotCameraData HdRobotComputeCameraData(const HdCamera& camera);

class HdRobotCamera final : public HdCamera
{
public:
  HdRobotCamera(const SdfPath& id, HdRobotRenderParam& scene);

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;
  void Finalize(HdRenderParam* renderParam) override;
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
  const HdRobotCameraData& GetCameraData() const;

private:
  HdRobotRenderParam&       _scene;
  mutable HdRobotCameraData _cameraData;
};

PXR_NAMESPACE_CLOSE_SCOPE
