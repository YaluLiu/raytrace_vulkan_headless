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
    DirtyParams = 1 << 1,
    AllDirty = DirtyTransform | DirtyParams,
  };
};

PXR_NAMESPACE_CLOSE_SCOPE
