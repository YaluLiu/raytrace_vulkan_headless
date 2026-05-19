#include "raster_renderer.hpp"

void RasterRenderer::loadModel(ModelLoader& loader, glm::mat4 transform)
{
  m_gpuScene.loadModel(loader, transform);
}

void RasterRenderer::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  m_gpuScene.loadTextureAssets(textureAssets);
}

void RasterRenderer::recreateTextureResources(const std::vector<TextureAsset>& textureAssets)
{
  vkDeviceWaitIdle(m_device);

  m_outputController.destroyPreviewPipeline();
  m_sceneDescriptors.destroy();

  m_gpuScene.recreateTextureResources(textureAssets);
  createDescriptorSetLayout();
  updateDescriptorSet();
  createPreviewRasterPipeline();
}

void RasterRenderer::createTextureImages(const VkCommandBuffer& cmdBuf,
                                         const std::vector<std::string>& textures,
                                         const std::vector<TextureAsset>& textureAssets)
{
  m_gpuScene.createTextureImages(cmdBuf, textures, textureAssets);
}
