#pragma once

#include <cstdint>

class Engine;

namespace engine
{
void createRenderResources(Engine& renderer);
void destroyRenderResources(Engine& renderer);
void resizeRenderTargets(Engine& renderer, int width, int height);

void createSceneDescriptors(Engine& renderer);
void updateSceneDescriptorBindings(Engine& renderer);
void createOutputPipelines(Engine& renderer);
void ensureFrameUniformCapacity(Engine& renderer, uint32_t slotCount);
uint32_t getRequiredFrameUniformSlots(const Engine& renderer);
}
