#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

void Renderer::uploadMesh(const MeshGeometry& geometry,
                                std::span<const Material> materials,
                                glm::mat4 transform)
{
  m_gpuScene.uploadMesh(geometry, materials, transform);
}

void Renderer::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  m_gpuScene.loadTextureAssets(textureAssets);
}

void Renderer::rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset>& textureAssets)
{
  vkDeviceWaitIdle(m_impl->device());

  m_outputController.destroyOutputPipelines();
  m_sceneDescriptors.destroy();

  m_gpuScene.rebuildTextureResources(textureAssets);
  engine::createSceneDescriptors(*this);
  engine::updateSceneDescriptorBindings(*this);
  engine::createOutputPipelines(*this);
}
