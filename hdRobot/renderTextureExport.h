#pragma once

#include <ModelLoader.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderPass.h>

#include <string>
#include <vector>

class HelloVulkan;
class HdRobotGlInteropCache;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

std::vector<TextureAsset>
ExportRegisteredTextures(const std::vector<TextureAsset> &registeredTextures);
bool CopyAovToRenderBuffer(const ::HelloVulkan &app, const TfToken &name,
                           HdRobotRenderBuffer *renderBuffer,
                           ::HdRobotGlInteropCache &glInteropCache);

PXR_NAMESPACE_CLOSE_SCOPE
