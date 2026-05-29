#pragma once

#include "../UsdRaySensorImaging/hydraSensor.h"
#include "backendHandles.h"
#include "camera.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/sprim.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam;

struct HdRobotLidarParams
{
  float azimuthStartDeg = -90.0f;
  float azimuthEndDeg = 90.0f;
  float azimuthStepDeg = 0.5f;
  float verticalStartDeg = -2.0f;
  float verticalEndDeg = -20.0f;
  float verticalStepDeg = 1.0f;
  float maxRange = 200.0f;
  float intensity = 1.0f;
};

struct HdRobotLidarSensorData
{
  std::string name;
  HdRobotCameraData camera;
  HdRobotLidarParams params;
};

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

  HdRobotLidarSensor(const SdfPath& id, HdRobotRenderParam& scene, HdRobotLidarSensorHandle handle);

  void Sync(HdSceneDelegate* sceneDelegate,
            HdRenderParam* renderParam,
            HdDirtyBits* dirtyBits) override;
  void Finalize(HdRenderParam* renderParam) override;
  HdDirtyBits GetInitialDirtyBitsMask() const override;

private:
  void _SyncParams(HdSceneDelegate* sceneDelegate);
  void _UpdateRenderParam();

  HdRobotRenderParam& _scene;
  HdRobotLidarSensorHandle _handle;
  GfMatrix4d _transform = GfMatrix4d(1.0);
  bool _enabled = true;
  HdRobotLidarParams _params;
};

PXR_NAMESPACE_CLOSE_SCOPE
