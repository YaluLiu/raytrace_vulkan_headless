#include "features/lidar/lidar_point_overlay_pipeline.hpp"

#include "features/preview/preview_raster_pipeline.hpp"

void LidarPointOverlayPipeline::setup(VkDevice device, nvvk::DebugUtil& debug)
{
  m_pipeline.setup(device, debug,
                   PointOverlayPipelineConfig{"LidarPointOverlay", "spv/lidar_overlay.vert.spv",
                                              "spv/lidar_overlay.frag.spv", "LiDAR overlay"});
}

void LidarPointOverlayPipeline::destroy()
{
  m_pipeline.destroy();
}

void LidarPointOverlayPipeline::destroyGraphicsPipeline()
{
  m_pipeline.destroyGraphicsPipeline();
}

bool LidarPointOverlayPipeline::ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                                const PreviewRasterPipeline& previewPipeline,
                                                VkBuffer sensorBuffer,
                                                VkBuffer pointBuffer)
{
  return m_pipeline.ensureResources(sceneDescriptorSetLayout, previewPipeline, sensorBuffer, pointBuffer);
}

void LidarPointOverlayPipeline::draw(const VkCommandBuffer& cmdBuf,
                                     const PreviewRasterPipeline& previewPipeline,
                                     VkDescriptorSet sceneDescriptorSet,
                                     const RasterLidarVisualizationConfig& config,
                                     uint32_t pointCount)
{
  if(!config.enabled)
  {
    return;
  }
  m_pipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, config.sensorIndex, config.pointSizePixels, pointCount);
}
