#pragma once

#include <ModelLoader.h>

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderPass.h>

#include <string>
#include <vector>

class HelloVulkan;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBuffer;

std::vector<TextureAsset> ExportRegisteredTextures(const std::vector<std::string>& texturePaths);
HdRobotRenderBuffer* GetPrimaryRenderBuffer(const HdRenderPassAovBindingVector& bindings);
void CopyAovToRenderBuffer(const ::HelloVulkan& app, const TfToken& name, HdRobotRenderBuffer* renderBuffer);

PXR_NAMESPACE_CLOSE_SCOPE
