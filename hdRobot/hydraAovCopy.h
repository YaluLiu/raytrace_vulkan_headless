#pragma once

#include "aovBridgeSpec.h"

#include <pxr/base/tf/token.h>

class Engine;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

namespace hdrobot {

class GlInteropCache;

struct AovCopyRequest
{
  TfToken aovName;
  Aov engineAov;
  AovCopyScaling scaling;
  HdRobotRenderBuffer *renderBuffer{nullptr};
};

bool CopyAovToRenderBuffer(const ::Engine &app,
                           const AovCopyRequest &request,
                           GlInteropCache &glInteropCache);

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
