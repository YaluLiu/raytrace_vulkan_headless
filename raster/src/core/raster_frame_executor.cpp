#include "core/raster_frame_executor.hpp"

#include "core/raster_renderer_internal.hpp"
#include "core/raster_renderer_resources.hpp"

#include <array>

namespace raster
{
namespace
{
enum class RasterFramePass
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

constexpr std::array<RasterFramePass, 8> kRasterFramePassSequence{
    RasterFramePass::UpdateUniforms,
    RasterFramePass::UpdateLights,
    RasterFramePass::RenderTileAovAtlas,
    RasterFramePass::GenerateLidarPointClouds,
    RasterFramePass::GenerateHeightScans,
    RasterFramePass::RenderPreviewAovs,
    RasterFramePass::OverlayLidarPointCloud,
    RasterFramePass::OverlayHeightScans,
};

void executeRasterFramePass(RasterRendererAccess::Impl& impl, const VkCommandBuffer& cmdBuf, RasterFramePass pass)
{
  switch(pass)
  {
    case RasterFramePass::UpdateUniforms:
      impl.viewUniforms.updateFrameUniformBuffer(cmdBuf, impl.sizeRef(), impl.gpuScene.getLightCount());
      break;
    case RasterFramePass::UpdateLights:
      impl.gpuScene.updateLightBuffer(cmdBuf);
      break;
    case RasterFramePass::RenderTileAovAtlas:
      impl.outputController.recordTileAtlas(cmdBuf, impl.gpuScene, impl.sceneDescriptors, impl.viewUniforms);
      break;
    case RasterFramePass::GenerateLidarPointClouds:
      impl.outputController.recordLidarPointClouds(cmdBuf, impl.gpuScene, impl.sceneDescriptors,
                                                   impl.outputController.getPreviewPipeline());
      break;
    case RasterFramePass::GenerateHeightScans:
      impl.outputController.recordHeightScans(cmdBuf, impl.gpuScene);
      break;
    case RasterFramePass::RenderPreviewAovs:
      impl.outputController.recordPreviewAovs(cmdBuf, impl.gpuScene, impl.sceneDescriptors);
      break;
    case RasterFramePass::OverlayLidarPointCloud:
      impl.outputController.recordLidarPointOverlay(cmdBuf, impl.sceneDescriptors);
      break;
    case RasterFramePass::OverlayHeightScans:
      impl.outputController.recordHeightScanOverlay(cmdBuf, impl.sceneDescriptors);
      break;
  }
}
} // namespace

void prepareFrame(RasterRenderer& renderer)
{
  ensureFrameUniformCapacity(renderer, getRequiredFrameUniformSlots(renderer));
  RasterRendererAccess::impl(renderer).gpuScene.flushRayTracingUpdates();
}

void recordFramePasses(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf)
{
  auto& impl = RasterRendererAccess::impl(renderer);
  for(const RasterFramePass pass : kRasterFramePassSequence)
  {
    executeRasterFramePass(impl, cmdBuf, pass);
  }
}

void finishFrame(RasterRenderer& renderer)
{
  RasterRendererAccess::impl(renderer).outputController.markTileAovAtlasConsumed();
}
} // namespace raster
