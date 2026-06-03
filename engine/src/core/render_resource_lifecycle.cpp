#include "core/render_resource_lifecycle.hpp"

#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

namespace engine
{
namespace
{
void updateTileFrameUniformDescriptor(EngineAccess::Impl& impl)
{
  impl.sceneDescriptors.updateTileFrameUniform(impl.viewUniforms.getTileFrameUniformDescriptorInfo());
}

void ensureViewUniformBuffers(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  ensureFrameUniformCapacity(renderer, getRequiredFrameUniformSlots(renderer));
  if(impl.viewUniforms.ensureTileFrameUniformBuffer())
  {
    updateTileFrameUniformDescriptor(impl);
  }
}
} // namespace

void RenderResourceLifecycle::createAll(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.createPreviewTargets(impl.sizeRef());
  impl.gpuScene.createLightBuffer();

  createSceneDescriptors(renderer);
  ensureViewUniformBuffers(renderer);
  impl.gpuScene.createObjectDescriptionBuffer();
  updateSceneDescriptorBindings(renderer);
  impl.gpuScene.createRayTracingResources();

  createOutputPipelines(renderer);
}

void RenderResourceLifecycle::destroyAll(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.destroy();
  impl.sceneDescriptors.destroy();
  impl.viewUniforms.destroy();
  impl.gpuScene.destroyRayTracingResources();
  impl.gpuScene.destroy();
  impl.alloc.deinit();
}

void RenderResourceLifecycle::recreateSceneDescriptorsAndOutputPipelines(Engine& renderer)
{
  auto& impl = EngineAccess::impl(renderer);
  impl.outputController.destroyOutputPipelines();
  impl.sceneDescriptors.destroy();
  createSceneDescriptors(renderer);
  updateSceneDescriptorBindings(renderer);
  createOutputPipelines(renderer);
}

void RenderResourceLifecycle::rebuildTexturesAndSceneBindings(Engine& renderer,
                                                              const std::vector<TextureAsset>& textureAssets)
{
  EngineAccess::impl(renderer).gpuScene.rebuildTextureResources(textureAssets);
  recreateSceneDescriptorsAndOutputPipelines(renderer);
}
} // namespace engine
