#pragma once

#include <vulkan/vulkan_core.h>

class Engine;

namespace engine
{
void prepareFrame(Engine& renderer);
void recordFramePasses(Engine& renderer, const VkCommandBuffer& cmdBuf);
void finishFrame(Engine& renderer);
}
