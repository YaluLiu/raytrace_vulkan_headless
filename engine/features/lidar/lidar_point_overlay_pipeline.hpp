#pragma once

#include <engine/lidar_types.hpp>

#include "features/point_overlay/point_overlay_pipeline.hpp"

#include <vulkan/vulkan_core.h>

class PreviewPipeline;

class LidarPointOverlayPipeline final
{
public:
  void setup(VkDevice device, nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  bool ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                       const PreviewPipeline& previewPipeline,
                       VkBuffer sensorBuffer,
                       VkBuffer pointBuffer);
  void draw(const VkCommandBuffer& cmdBuf,
            const PreviewPipeline& previewPipeline,
            VkDescriptorSet sceneDescriptorSet,
            const LidarVisualizationConfig& config,
            uint32_t pointCount);

private:
  PointOverlayPipeline m_pipeline;
};
