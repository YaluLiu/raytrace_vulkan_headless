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
  RenderPreviewAovs,
};

constexpr std::array<RasterFramePass, 4> kRasterFramePassSequence{
    RasterFramePass::UpdateUniforms,
    RasterFramePass::UpdateLights,
    RasterFramePass::RenderTileAovAtlas,
    RasterFramePass::RenderPreviewAovs,
};

void executeRasterFramePass(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf, RasterFramePass pass)
{
  switch(pass)
  {
    case RasterFramePass::UpdateUniforms:
      renderer.updateUniformBuffer(cmdBuf);
      break;
    case RasterFramePass::UpdateLights:
      renderer.updateLightBuffer(cmdBuf);
      break;
    case RasterFramePass::RenderTileAovAtlas:
      renderer.renderTileAovAtlas(cmdBuf);
      break;
    case RasterFramePass::RenderPreviewAovs:
      renderer.renderPreviewAovs(cmdBuf);
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
