#pragma once

#include "aovBridgeSpec.h"

#include <pxr/base/tf/token.h>

class RasterRenderer;
class HdRobotGlInteropCache;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

struct HdRobotAovCopyRequest
{
  TfToken aovName;
  RasterAov rasterAov;
  HdRobotAovCopyScaling scaling;
  HdRobotRenderBuffer *renderBuffer{nullptr};
};

bool CopyAovToRenderBuffer(const ::RasterRenderer &app,
                           const HdRobotAovCopyRequest &request,
                           ::HdRobotGlInteropCache &glInteropCache);

PXR_NAMESPACE_CLOSE_SCOPE
