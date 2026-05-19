#include "raster_renderer.hpp"

void RasterRenderer::createPreviewRasterPipeline()
{
  m_outputController.createPreviewPipeline(m_sceneDescriptors.layout());
}

void RasterRenderer::renderPreviewAovs(const VkCommandBuffer& cmdBuf)
{
  m_outputController.recordPreviewAovs(cmdBuf, m_gpuScene, m_sceneDescriptors);
}

void RasterRenderer::createDescriptorSetLayout()
{
  m_sceneDescriptors.create(m_device, m_gpuScene.getTextureDescriptorCount(), true);
}

void RasterRenderer::updateDescriptorSet()
{
  VkDescriptorBufferInfo dbiSceneDesc{m_gpuScene.getObjectDescriptionBuffer().buffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo dbiLights{m_gpuScene.getLightBuffer().buffer, 0, VK_WHOLE_SIZE};
  m_sceneDescriptors.update(m_viewUniforms.getFrameUniformDescriptorInfo(), dbiSceneDesc, dbiLights,
                            m_viewUniforms.getTileFrameUniformDescriptorInfo(), m_gpuScene.getTextures());
}
