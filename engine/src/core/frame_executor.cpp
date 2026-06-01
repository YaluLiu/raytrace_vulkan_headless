#include "core/frame_executor.hpp"

#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

#include <array>

namespace engine
{
namespace
{
enum class FramePass
{
  UpdateUniforms,
  UpdateLights,
  RenderTileAovAtlas,
  GenerateLidarPointClouds,
  GenerateHeightScans,
  RenderPreviewAovs,
  OverlayLidarPointCloud,
  OverlayHeightScans,
};

constexpr std::array<FramePass, 8> kFramePassSequence{
    FramePass::UpdateUniforms,
    FramePass::UpdateLights,
    FramePass::RenderTileAovAtlas,
    FramePass::GenerateLidarPointClouds,
    FramePass::GenerateHeightScans,
    FramePass::RenderPreviewAovs,
    FramePass::OverlayLidarPointCloud,
    FramePass::OverlayHeightScans,
};

void executeFramePass(RendererAccess::Impl& impl, const VkCommandBuffer& cmdBuf, FramePass pass)
{
  switch(pass)
  {
    case FramePass::UpdateUniforms:
      impl.viewUniforms.updateFrameUniformBuffer(cmdBuf, impl.sizeRef(), impl.gpuScene.getLightCount());
      break;
    case FramePass::UpdateLights:
      impl.gpuScene.updateLightBuffer(cmdBuf);
      break;
    case FramePass::RenderTileAovAtlas:
      impl.outputController.recordTileAtlas(cmdBuf, impl.gpuScene, impl.sceneDescriptors, impl.viewUniforms);
      break;
    case FramePass::GenerateLidarPointClouds:
      impl.outputController.recordLidarPointClouds(cmdBuf, impl.gpuScene, impl.sceneDescriptors,
                                                   impl.outputController.getPreviewPipeline());
      break;
    case FramePass::GenerateHeightScans:
      impl.outputController.recordHeightScans(cmdBuf, impl.gpuScene);
      break;
    case FramePass::RenderPreviewAovs:
      impl.outputController.recordPreviewAovs(cmdBuf, impl.gpuScene, impl.sceneDescriptors);
      break;
    case FramePass::OverlayLidarPointCloud:
      impl.outputController.recordLidarPointOverlay(cmdBuf, impl.sceneDescriptors);
      break;
    case FramePass::OverlayHeightScans:
      impl.outputController.recordHeightScanOverlay(cmdBuf, impl.sceneDescriptors);
      break;
  }
}
} // namespace

void prepareFrame(Renderer& renderer)
{
  ensureFrameUniformCapacity(renderer, getRequiredFrameUniformSlots(renderer));
  RendererAccess::impl(renderer).gpuScene.flushRayTracingUpdates();
}

void recordFramePasses(Renderer& renderer, const VkCommandBuffer& cmdBuf)
{
  auto& impl = RendererAccess::impl(renderer);
  for(const FramePass pass : kFramePassSequence)
  {
    executeFramePass(impl, cmdBuf, pass);
  }
}

void finishFrame(Renderer& renderer)
{
  RendererAccess::impl(renderer).outputController.markTileAovAtlasConsumed();
}
} // namespace engine
