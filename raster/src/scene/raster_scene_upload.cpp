#include "core/raster_renderer_internal.hpp"

void RasterRenderer::uploadMeshFromLoader(ModelLoader& loader, glm::mat4 transform)
{
  m_gpuScene.uploadMeshFromLoader(loader, transform);
}

void RasterRenderer::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  m_gpuScene.loadTextureAssets(textureAssets);
}

void RasterRenderer::rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset>& textureAssets)
{
  vkDeviceWaitIdle(m_impl->device());

  m_outputController.destroyOutputPipelines();
  m_sceneDescriptors.destroy();

  m_gpuScene.rebuildTextureResources(textureAssets);
  createSceneDescriptors();
  updateSceneDescriptorBindings();
  createPreviewOutputPipeline();
}
