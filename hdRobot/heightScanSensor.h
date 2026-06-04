#pragma once

#include "../UsdRaySensorImaging/hydraSensor.h"
#include "backendHandles.h"
#include "camera.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/sprim.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotSceneStore;

struct HdRobotHeightScanParams
{
  float uStart = -0.2f;
  float uEnd = 0.2f;
  float uStep = 0.1f;
  float vStart = -0.5f;
  float vEnd = 0.5f;
  float vStep = 0.1f;
  glm::vec3 gravityDirectionWs = glm::vec3(0.0f, 0.0f, -1.0f);
  float maxRange = 5.0f;
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

  HdRobotHeightScanSensor(const SdfPath& id, HdRobotSceneStore& sceneStore, HdRobotHeightScanSensorHandle handle);

  void Sync(HdSceneDelegate* sceneDelegate,
            HdRenderParam* renderParam,
            HdDirtyBits* dirtyBits) override;
  void Finalize(HdRenderParam* renderParam) override;
  HdDirtyBits GetInitialDirtyBitsMask() const override;

private:
  void _SyncParams(HdSceneDelegate* sceneDelegate);
  void _UpdateRenderParam();

  HdRobotSceneStore& _sceneStore;
  HdRobotHeightScanSensorHandle _handle;
  GfMatrix4d _transform = GfMatrix4d(1.0);
  bool _enabled = true;
  HdRobotHeightScanParams _params;
};

PXR_NAMESPACE_CLOSE_SCOPE
