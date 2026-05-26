#pragma once

#include <pxr/imaging/hd/camera.h>
#include <glm/glm.hpp>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam;

struct HdRobotLidarParams
{
  float azimuthMinDeg = -90.0f;
  float azimuthMaxDeg = 90.0f;
  float azimuthStepDeg = 0.5f;
  float verticalMinDeg = -2.0f;
  float verticalMaxDeg = -20.0f;
  float verticalStepDeg = 1.0f;
  float maxDistance = 200.0f;
  float intensity = 1.0f;
};

struct HdRobotHeightScanParams
{
  float minX = -10.0f;
  float maxX = 10.0f;
  float stepX = 0.1f;
  float minZ = -10.0f;
  float maxZ = 10.0f;
  float stepZ = 0.1f;
  glm::vec3 rayDirection = glm::vec3(0.0f, 0.0f, -1.0f);
  float maxDistance = 200.0f;
};

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

struct HdRobotLidarSensorData
{
  std::string name;
  HdRobotCameraData camera;
  HdRobotLidarParams params;
};

struct HdRobotHeightScanSensorData
{
  std::string name;
  HdRobotCameraData camera;
  HdRobotHeightScanParams params;
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
