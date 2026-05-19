#pragma once

#include <pxr/base/tf/token.h>

class RasterRenderer;
class HdRobotGlInteropCache;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

bool CopyAovToRenderBuffer(const ::RasterRenderer &app,
                           const TfToken &name,
                           HdRobotRenderBuffer *renderBuffer,
                           ::HdRobotGlInteropCache &glInteropCache);

PXR_NAMESPACE_CLOSE_SCOPE
