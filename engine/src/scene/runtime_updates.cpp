#include "core/renderer_internal.hpp"

void Renderer::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  m_gpuScene.updateMaterialsAtRuntime(updates);
}

void Renderer::addLight(const Light& light)
{
  m_gpuScene.addLight(light);
}

void Renderer::clearLights()
{
  m_gpuScene.clearLights();
}
