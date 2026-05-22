#include "core/raster_renderer_internal.hpp"

void RasterRenderer::createPreviewOutputPipeline()
{
  m_outputController.rebuildPipelinesForSceneLayout(m_sceneDescriptors.layout());
}

void RasterRenderer::recordPreviewAovs(const VkCommandBuffer& cmdBuf)
{
  m_outputController.recordPreviewAovs(cmdBuf, m_gpuScene, m_sceneDescriptors);
}

void RasterRenderer::recordLidarPointClouds(const VkCommandBuffer& cmdBuf)
{
  m_outputController.recordLidarPointClouds(cmdBuf, m_gpuScene, m_sceneDescriptors,
                                            m_outputController.getPreviewPipeline());
}

void RasterRenderer::recordHeightScans(const VkCommandBuffer& cmdBuf)
{
  m_outputController.recordHeightScans(cmdBuf, m_gpuScene);
}

void RasterRenderer::recordLidarPointOverlay(const VkCommandBuffer& cmdBuf)
{
  m_outputController.recordLidarPointOverlay(cmdBuf, m_sceneDescriptors);
}

void RasterRenderer::createSceneDescriptors()
{
  m_sceneDescriptors.create(m_impl->device(), m_gpuScene.getTextureDescriptorCount(), true);
}

void RasterRenderer::updateSceneDescriptorBindings()
{
  VkDescriptorBufferInfo dbiSceneDesc{m_gpuScene.getObjectDescriptionBuffer().buffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo dbiLights{m_gpuScene.getLightBuffer().buffer, 0, VK_WHOLE_SIZE};
  m_sceneDescriptors.update(m_viewUniforms.getFrameUniformDescriptorInfo(), dbiSceneDesc, dbiLights,
                            m_viewUniforms.getTileFrameUniformDescriptorInfo(), m_gpuScene.getTextures());
}
