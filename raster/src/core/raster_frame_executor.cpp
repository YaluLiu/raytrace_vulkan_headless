#include "core/raster_frame_executor.hpp"

#include <raster/raster_renderer.hpp>

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
  RenderPreviewAovs,
  OverlayLidarPointCloud,
};

constexpr std::array<RasterFramePass, 6> kRasterFramePassSequence{
    RasterFramePass::UpdateUniforms,
    RasterFramePass::UpdateLights,
    RasterFramePass::RenderTileAovAtlas,
    RasterFramePass::GenerateLidarPointClouds,
    RasterFramePass::RenderPreviewAovs,
    RasterFramePass::OverlayLidarPointCloud,
};

void executeRasterFramePass(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf, RasterFramePass pass)
{
  switch(pass)
  {
    case RasterFramePass::UpdateUniforms:
      renderer.recordFrameUniformUpdate(cmdBuf);
      break;
    case RasterFramePass::UpdateLights:
      renderer.updateLightBuffer(cmdBuf);
      break;
    case RasterFramePass::RenderTileAovAtlas:
      renderer.recordTileAovAtlas(cmdBuf);
      break;
    case RasterFramePass::GenerateLidarPointClouds:
      renderer.recordLidarPointClouds(cmdBuf);
      break;
    case RasterFramePass::RenderPreviewAovs:
      renderer.recordPreviewAovs(cmdBuf);
      break;
    case RasterFramePass::OverlayLidarPointCloud:
      renderer.recordLidarPointOverlay(cmdBuf);
      break;
  }
}
} // namespace

void recordFramePasses(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf)
{
  for(const RasterFramePass pass : kRasterFramePassSequence)
  {
    executeRasterFramePass(renderer, cmdBuf, pass);
  }
}
} // namespace raster
