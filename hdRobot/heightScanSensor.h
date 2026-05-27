#pragma once

#include "../UsdRaySensor/hydraSensor.h"
#include "camera.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/sprim.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam;

struct HdRobotHeightScanParams
{
  float uStart = -10.0f;
  float uEnd = 10.0f;
  float uStep = 0.1f;
  float vStart = -10.0f;
  float vEnd = 10.0f;
  float vStep = 0.1f;
  glm::vec3 gravityDirectionWs = glm::vec3(0.0f, 0.0f, -1.0f);
  float maxRange = 200.0f;
};

struct HdRobotHeightScanSensorData
{
  std::string name;
  HdRobotCameraData camera;
  HdRobotHeightScanParams params;
};

class HdRobotHeightScanSensor final : public HdSprim
{
public:
  enum DirtyBits : HdDirtyBits
  {
    Clean = 0,
    DirtyTransform = HdRaySensorDirtyBits::DirtyTransform,
    DirtyParams = HdRaySensorDirtyBits::DirtyParams,
    AllDirty = HdRaySensorDirtyBits::AllDirty,
  };

  HdRobotHeightScanSensor(const SdfPath& id, HdRobotRenderParam& scene);

  void Sync(HdSceneDelegate* sceneDelegate,
            HdRenderParam* renderParam,
            HdDirtyBits* dirtyBits) override;
  void Finalize(HdRenderParam* renderParam) override;
  HdDirtyBits GetInitialDirtyBitsMask() const override;

private:
  void _SyncParams(HdSceneDelegate* sceneDelegate);
  void _UpdateRenderParam();

  HdRobotRenderParam& _scene;
  GfMatrix4d _transform = GfMatrix4d(1.0);
  bool _enabled = true;
  HdRobotHeightScanParams _params;
};

PXR_NAMESPACE_CLOSE_SCOPE
