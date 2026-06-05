#include "core/renderer_resources.hpp"

#include "core/render_resource_lifecycle.hpp"
#include "core/renderer_internal.hpp"

namespace engine
{
namespace
{
void refreshFrameUniformDescriptorBinding(EngineAccess::Impl& impl)
{
  impl.sceneDescriptors.updateFrameUniform(impl.viewUniforms.getFrameUniformDescriptorInfo());
}

} // namespace

uint32_t getRequiredFrameUniformSlots(const Engine& /*renderer*/)
{
  return 1;
}

void ensureFrameUniformCapacity(Engine& renderer, uint32_t slotCount)
{
  auto& impl = EngineAccess::impl(renderer);
  if(impl.viewUniforms.ensureFrameUniformCapacity(slotCount))
  {
    refreshFrameUniformDescriptorBinding(impl);
  }
}

void createRenderResources(Engine& renderer)
{
  RenderResourceLifecycle::createResourcesInDependencyOrder(renderer);
}

void destroyRenderResources(Engine& renderer)
{
  RenderResourceLifecycle::destroyResourcesInShutdownOrder(renderer);
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
