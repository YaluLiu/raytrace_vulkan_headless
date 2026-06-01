#include "core/raster_renderer_internal.hpp"

void RasterRenderer::recordTileAovAtlas(const VkCommandBuffer& cmdBuf)
{
  m_outputController.recordTileAtlas(cmdBuf, m_gpuScene, m_sceneDescriptors, m_viewUniforms);
}

void RasterRenderer::markTileAovAtlasConsumed(const std::string& outputDirectory)
{
  m_outputController.markTileAovAtlasConsumed(outputDirectory);
}
