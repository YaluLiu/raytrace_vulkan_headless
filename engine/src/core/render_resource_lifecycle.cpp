#include "core/render_resource_lifecycle.hpp"

#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

namespace engine
{
namespace
{
void refreshTileFrameUniformDescriptorBinding(EngineAccess::Impl& impl)
{
  impl.sceneDescriptors.updateTileFrameUniform(impl.viewUniforms.getTileFrameUniformDescriptorInfo());
}

void createSceneDescriptorsForCurrentScene(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.sceneDescriptors.create(impl.device(), impl.gpuScene.getTextureDescriptorCount(), true);
}

void refreshSceneDescriptorBindings(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  VkDescriptorBufferInfo dbiSceneDesc{impl.gpuScene.getObjectDescriptionBuffer().buffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo dbiLights{impl.gpuScene.getLightBuffer().buffer, 0, VK_WHOLE_SIZE};
  impl.sceneDescriptors.update(impl.viewUniforms.getFrameUniformDescriptorInfo(), dbiSceneDesc, dbiLights,
                               impl.viewUniforms.getTileFrameUniformDescriptorInfo(), impl.gpuScene.getTextures());
}

void rebuildOutputPipelinesForCurrentSceneLayout(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.rebuildPipelinesForSceneLayout(impl.sceneDescriptors.layout());
}

void ensureViewUniformBuffersAndDescriptorBindings(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  ensureFrameUniformCapacity(renderer, getRequiredFrameUniformSlots(renderer));
  if(impl.viewUniforms.ensureTileFrameUniformBuffer())
  {
    refreshTileFrameUniformDescriptorBinding(impl);
  }
}

void rebuildSceneDescriptorsThenOutputPipelines(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.destroyOutputPipelines();
  impl.sceneDescriptors.destroy();
  createSceneDescriptorsForCurrentScene(renderer);
  refreshSceneDescriptorBindings(renderer);
  rebuildOutputPipelinesForCurrentSceneLayout(renderer);
}
} // namespace

void RenderResourceLifecycle::createResourcesInDependencyOrder(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.createPreviewTargets(impl.sizeRef());
  impl.gpuScene.createLightBuffer();

  createSceneDescriptorsForCurrentScene(renderer);
  ensureViewUniformBuffersAndDescriptorBindings(renderer);
  impl.gpuScene.createObjectDescriptionBuffer();
  refreshSceneDescriptorBindings(renderer);
  impl.gpuScene.createRayTracingResources();

  rebuildOutputPipelinesForCurrentSceneLayout(renderer);
}

void RenderResourceLifecycle::destroyResourcesInShutdownOrder(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.destroy();
  impl.sceneDescriptors.destroy();
  impl.viewUniforms.destroy();
  impl.gpuScene.destroyRayTracingResources();
  impl.gpuScene.destroy();
  impl.alloc.deinit();
}

void RenderResourceLifecycle::rebuildTexturesAndSceneBindings(Engine& renderer,
                                                              const std::vector<TextureAsset>& textureAssets)
{
  EngineAccess::impl(renderer).gpuScene.rebuildTextureResources(textureAssets);
  rebuildSceneDescriptorsThenOutputPipelines(renderer);
}
} // namespace engine
