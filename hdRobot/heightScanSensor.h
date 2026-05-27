#pragma once

#include "../UsdRaySensor/hydraSensor.h"
#include "camera.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/sprim.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam;

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
