#pragma once

#include <vulkan/vulkan_core.h>

class RasterRenderer;

namespace raster
{
void prepareFrame(RasterRenderer& renderer);
void recordFramePasses(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf);
void finishFrame(RasterRenderer& renderer);
}
