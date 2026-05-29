#pragma once

#include <pxr/pxr.h>
#include <raster/texture_asset.hpp>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

std::vector<TextureAsset>
ExportRegisteredTextures(const std::vector<TextureAsset> &registeredTextures);

PXR_NAMESPACE_CLOSE_SCOPE
