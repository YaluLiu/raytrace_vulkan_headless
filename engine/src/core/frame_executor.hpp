#pragma once

#include <vulkan/vulkan_core.h>

class Renderer;

namespace engine
{
void prepareFrame(Renderer& renderer);
void recordFramePasses(Renderer& renderer, const VkCommandBuffer& cmdBuf);
void finishFrame(Renderer& renderer);
}
