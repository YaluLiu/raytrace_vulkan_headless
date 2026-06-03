#include "core/renderer_resources.hpp"

#include "core/render_resource_lifecycle.hpp"
#include "core/renderer_internal.hpp"

namespace engine
{
namespace
{
void updateFrameUniformDescriptor(EngineAccess::Impl& impl)
{
  impl.sceneDescriptors.updateFrameUniform(impl.viewUniforms.getFrameUniformDescriptorInfo());
}

} // namespace

void createSceneDescriptors(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.sceneDescriptors.create(impl.device(), impl.gpuScene.getTextureDescriptorCount(), true);
}

void updateSceneDescriptorBindings(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  VkDescriptorBufferInfo dbiSceneDesc{impl.gpuScene.getObjectDescriptionBuffer().buffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo dbiLights{impl.gpuScene.getLightBuffer().buffer, 0, VK_WHOLE_SIZE};
  impl.sceneDescriptors.update(impl.viewUniforms.getFrameUniformDescriptorInfo(), dbiSceneDesc, dbiLights,
                               impl.viewUniforms.getTileFrameUniformDescriptorInfo(), impl.gpuScene.getTextures());
}

void createOutputPipelines(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.rebuildPipelinesForSceneLayout(impl.sceneDescriptors.layout());
}

uint32_t getRequiredFrameUniformSlots(const Engine& /*renderer*/)
{
  return 1;
}

void ensureFrameUniformCapacity(Engine& renderer, uint32_t slotCount)
{
  auto& impl = EngineAccess::impl(renderer);
  if(impl.viewUniforms.ensureFrameUniformCapacity(slotCount))
  {
    updateFrameUniformDescriptor(impl);
  }
}

void createRenderResources(Engine& renderer)
{
  RenderResourceLifecycle::createAll(renderer);
}

void destroyRenderResources(Engine& renderer)
{
  RenderResourceLifecycle::destroyAll(renderer);
}

void resizeRenderTargets(Engine& renderer, int width, int height)
{
  if(width <= 0 || height <= 0)
  {
    return;
  }

  auto& impl = EngineAccess::impl(renderer);
  if(width == static_cast<int>(impl.sizeRef().width) && height == static_cast<int>(impl.sizeRef().height))
  {
    return;
  }

  impl.sizeRef().width = width;
  impl.sizeRef().height = height;
  impl.outputController.createPreviewTargets(impl.sizeRef());
}
} // namespace engine
