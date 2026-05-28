#pragma once

#include "api.h"

#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/types.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

#define HD_RAY_SENSOR_PRIM_TYPE_TOKENS \
  ((lidarSensor, "lidarSensor")) \
  ((heightScanSensor, "heightScanSensor"))

TF_DECLARE_PUBLIC_TOKENS(HdRaySensorPrimTypeTokens,
                         USDRAYSENSORIMAGING_API,
                         HD_RAY_SENSOR_PRIM_TYPE_TOKENS);

struct HdRaySensorDirtyBits
{
  enum : HdDirtyBits
  {
    Clean = 0,
    DirtyTransform = 1 << 0,
    // Hydra's HdChangeTracker::AllDirty masks out bit 1 for Rprim varying state.
    // Keep sensor params off bit 1 so full-dirty sprim syncs still reload them.
    DirtyParams = 1 << 2,
    AllDirty = DirtyTransform | DirtyParams,
  };
};

PXR_NAMESPACE_CLOSE_SCOPE
