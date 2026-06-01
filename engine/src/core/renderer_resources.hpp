#pragma once

#include <cstdint>

class Renderer;

namespace engine
{
void createRenderResources(Renderer& renderer);
void destroyRenderResources(Renderer& renderer);
void resizeRenderTargets(Renderer& renderer, int width, int height);

void createSceneDescriptors(Renderer& renderer);
void updateSceneDescriptorBindings(Renderer& renderer);
void createOutputPipelines(Renderer& renderer);
void ensureFrameUniformCapacity(Renderer& renderer, uint32_t slotCount);
uint32_t getRequiredFrameUniformSlots(const Renderer& renderer);
}
