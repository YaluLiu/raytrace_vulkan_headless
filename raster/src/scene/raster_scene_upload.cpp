#include "core/raster_renderer_internal.hpp"
#include "core/raster_renderer_resources.hpp"

void RasterRenderer::uploadMesh(const RasterMeshGeometry& geometry,
                                std::span<const RasterMaterial> materials,
                                glm::mat4 transform)
{
  m_gpuScene.uploadMesh(geometry, materials, transform);
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
  raster::createSceneDescriptors(*this);
  raster::updateSceneDescriptorBindings(*this);
  raster::createOutputPipelines(*this);
}
