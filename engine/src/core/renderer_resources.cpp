#include "core/renderer_resources.hpp"

#include "core/renderer_internal.hpp"

namespace engine
{
namespace
{
void updateFrameUniformDescriptor(RendererAccess::Impl& impl)
{
  impl.sceneDescriptors.updateFrameUniform(impl.viewUniforms.getFrameUniformDescriptorInfo());
}

void updateTileFrameUniformDescriptor(RendererAccess::Impl& impl)
{
  impl.sceneDescriptors.updateTileFrameUniform(impl.viewUniforms.getTileFrameUniformDescriptorInfo());
}

void ensureViewUniformBuffers(Renderer& renderer)
{
  auto& impl = RendererAccess::impl(renderer);
  ensureFrameUniformCapacity(renderer, getRequiredFrameUniformSlots(renderer));
  if(impl.viewUniforms.ensureTileFrameUniformBuffer())
  {
    updateTileFrameUniformDescriptor(impl);
  }
}
} // namespace

void createSceneDescriptors(Renderer& renderer)
{
  auto& impl = RendererAccess::impl(renderer);
  impl.sceneDescriptors.create(impl.device(), impl.gpuScene.getTextureDescriptorCount(), true);
}

void updateSceneDescriptorBindings(Renderer& renderer)
{
  auto& impl = RendererAccess::impl(renderer);
  VkDescriptorBufferInfo dbiSceneDesc{impl.gpuScene.getObjectDescriptionBuffer().buffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo dbiLights{impl.gpuScene.getLightBuffer().buffer, 0, VK_WHOLE_SIZE};
  impl.sceneDescriptors.update(impl.viewUniforms.getFrameUniformDescriptorInfo(), dbiSceneDesc, dbiLights,
                               impl.viewUniforms.getTileFrameUniformDescriptorInfo(), impl.gpuScene.getTextures());
}

void createOutputPipelines(Renderer& renderer)
{
  auto& impl = RendererAccess::impl(renderer);
  impl.outputController.rebuildPipelinesForSceneLayout(impl.sceneDescriptors.layout());
}

uint32_t getRequiredFrameUniformSlots(const Renderer& /*renderer*/)
{
  return 1;
}

void ensureFrameUniformCapacity(Renderer& renderer, uint32_t slotCount)
{
  auto& impl = RendererAccess::impl(renderer);
  if(impl.viewUniforms.ensureFrameUniformCapacity(slotCount))
  {
    updateFrameUniformDescriptor(impl);
  }
}

void createRenderResources(Renderer& renderer)
{
  auto& impl = RendererAccess::impl(renderer);
  impl.outputController.createPreviewTargets(impl.sizeRef());
  impl.gpuScene.createLightBuffer();

  createSceneDescriptors(renderer);
  ensureViewUniformBuffers(renderer);
  impl.gpuScene.createObjectDescriptionBuffer();
  updateSceneDescriptorBindings(renderer);
  impl.gpuScene.createRayTracingResources();

  createOutputPipelines(renderer);
}

void destroyRenderResources(Renderer& renderer)
{
  auto& impl = RendererAccess::impl(renderer);
  impl.outputController.destroy();
  impl.sceneDescriptors.destroy();
  impl.viewUniforms.destroy();
  impl.gpuScene.destroyRayTracingResources();
  impl.gpuScene.destroy();
  impl.alloc.deinit();
}

void resizeRenderTargets(Renderer& renderer, int width, int height)
{
  if(width <= 0 || height <= 0)
  {
    return;
  }

  auto& impl = RendererAccess::impl(renderer);
  if(width == static_cast<int>(impl.sizeRef().width) && height == static_cast<int>(impl.sizeRef().height))
  {
    return;
  }

  impl.sizeRef().width = width;
  impl.sizeRef().height = height;
  impl.outputController.createPreviewTargets(impl.sizeRef());
}
} // namespace engine
