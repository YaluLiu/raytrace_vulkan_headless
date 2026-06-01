#pragma once

#include <raster/lidar_types.hpp>

#include "features/point_overlay/point_overlay_pipeline.hpp"

#include <vulkan/vulkan_core.h>

class PreviewRasterPipeline;

class LidarPointOverlayPipeline final
{
public:
  void setup(VkDevice device, nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  bool ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                       const PreviewRasterPipeline& previewPipeline,
                       VkBuffer sensorBuffer,
                       VkBuffer pointBuffer);
  void draw(const VkCommandBuffer& cmdBuf,
            const PreviewRasterPipeline& previewPipeline,
            VkDescriptorSet sceneDescriptorSet,
            const RasterLidarVisualizationConfig& config,
            uint32_t pointCount);

private:
  PointOverlayPipeline m_pipeline;
};
