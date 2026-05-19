#pragma once

#include <ModelLoader.h>
#include <pxr/pxr.h>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

std::vector<TextureAsset>
ExportRegisteredTextures(const std::vector<TextureAsset> &registeredTextures);

PXR_NAMESPACE_CLOSE_SCOPE
