#pragma once

#include <vulkan/vulkan_core.h>

class RasterRenderer;

namespace raster
{
void recordFramePasses(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf);
}
