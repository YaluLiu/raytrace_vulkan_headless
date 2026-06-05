#pragma once

#include <cstdint>

class Engine;

namespace engine
{
// Core-facing wrappers used by the Engine facade/session layer.
void createRenderResources(Engine& renderer);
void destroyRenderResources(Engine& renderer);
void resizeRenderTargets(Engine& renderer, int width, int height);

// Shared frame-uniform capacity helpers used by lifecycle setup and frame prep.
void ensureFrameUniformCapacity(Engine& renderer, uint32_t slotCount);
uint32_t getRequiredFrameUniformSlots(const Engine& renderer);
}
