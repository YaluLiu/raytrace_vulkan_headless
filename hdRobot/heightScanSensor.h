#pragma once

#include "camera.h"

#include <pxr/imaging/hd/sprim.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam;

class HdRobotHeightScanSensor final : public HdSprim
{
public:
  enum DirtyBits : HdDirtyBits
  {
    Clean = 0,
    DirtyTransform = 1 << 0,
    DirtyParams = 1 << 1,
    AllDirty = DirtyTransform | DirtyParams,
  };

  HdRobotHeightScanSensor(const SdfPath& id, HdRobotRenderParam& scene);

  void Sync(HdSceneDelegate* sceneDelegate,
            HdRenderParam* renderParam,
            HdDirtyBits* dirtyBits) override;
  void Finalize(HdRenderParam* renderParam) override;
  HdDirtyBits GetInitialDirtyBitsMask() const override;

private:
  HdRobotRenderParam& _scene;
};

PXR_NAMESPACE_CLOSE_SCOPE
