#pragma once

#include <cstdint>

class RasterRenderer;

namespace raster
{
void createRenderResources(RasterRenderer& renderer);
void destroyRenderResources(RasterRenderer& renderer);
void resizeRenderTargets(RasterRenderer& renderer, int width, int height);

void createSceneDescriptors(RasterRenderer& renderer);
void updateSceneDescriptorBindings(RasterRenderer& renderer);
void createOutputPipelines(RasterRenderer& renderer);
void ensureFrameUniformCapacity(RasterRenderer& renderer, uint32_t slotCount);
uint32_t getRequiredFrameUniformSlots(const RasterRenderer& renderer);
}
