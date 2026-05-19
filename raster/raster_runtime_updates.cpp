#include "raster_renderer.hpp"

void RasterRenderer::updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate>& updates)
{
  m_gpuScene.updateMaterialsAtRuntime(updates);
}

void RasterRenderer::addLight(const Light& light)
{
  m_gpuScene.addLight(light);
}

void RasterRenderer::clearLights()
{
  m_gpuScene.clearLights();
}

void RasterRenderer::createLightBuffer()
{
  m_gpuScene.createLightBuffer();
}

void RasterRenderer::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  m_gpuScene.updateLightBuffer(cmdBuf);
}
