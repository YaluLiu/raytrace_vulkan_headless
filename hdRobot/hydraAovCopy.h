#pragma once

#include "aovBridgeSpec.h"

#include <pxr/base/tf/token.h>

class Renderer;
class HdRobotGlInteropCache;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

struct HdRobotAovCopyRequest
{
  TfToken aovName;
  Aov engineAov;
  HdRobotAovCopyScaling scaling;
  HdRobotRenderBuffer *renderBuffer{nullptr};
};

bool CopyAovToRenderBuffer(const ::Renderer &app,
                           const HdRobotAovCopyRequest &request,
                           ::HdRobotGlInteropCache &glInteropCache);

PXR_NAMESPACE_CLOSE_SCOPE
