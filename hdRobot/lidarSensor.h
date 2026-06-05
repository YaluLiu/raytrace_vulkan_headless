#pragma once

#include "../UsdRaySensorImaging/hydraSensor.h"
#include "backendHandles.h"
#include "camera.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/sprim.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

class SceneStore;

struct LidarParams
{
  float azimuthStartDeg = -90.0f;
  float azimuthEndDeg = 90.0f;
  float azimuthStepDeg = 0.5f;
  float verticalStartDeg = -85.0f;
  float verticalEndDeg = -35.0f;
  float verticalStepDeg = 5.0f;
  float maxRange = 5.0f;
  float intensity = 1.0f;
};

struct LidarSensorData
{
  std::string name;
  CameraData camera;
  LidarParams params;
};

} // namespace hdrobot

class HdRobotLidarSensor final : public HdSprim
{
public:
  enum DirtyBits : HdDirtyBits
  {
    Clean = 0,
    DirtyTransform = HdRaySensorDirtyBits::DirtyTransform,
    DirtyParams = HdRaySensorDirtyBits::DirtyParams,
    AllDirty = HdRaySensorDirtyBits::AllDirty,
  };

  HdRobotLidarSensor(const SdfPath& id, hdrobot::SceneStore& sceneStore, hdrobot::LidarSensorHandle handle);

  void Sync(HdSceneDelegate* sceneDelegate,
            HdRenderParam* renderParam,
            HdDirtyBits* dirtyBits) override;
  void Finalize(HdRenderParam* renderParam) override;
  HdDirtyBits GetInitialDirtyBitsMask() const override;

private:
  void _SyncParams(HdSceneDelegate* sceneDelegate);
  void _UpdateRenderParam();

  hdrobot::SceneStore& _sceneStore;
  hdrobot::LidarSensorHandle _handle;
  GfMatrix4d _transform = GfMatrix4d(1.0);
  bool _enabled = true;
  hdrobot::LidarParams _params;
};

PXR_NAMESPACE_CLOSE_SCOPE
