#pragma once

#include <engine/renderer_types.hpp>

#include "features/sensor_common/sensor_generation_pipeline.hpp"

#include <vulkan/vulkan_core.h>

class HeightScanGenerationPipeline final
{
public:
  void setup(VkDevice device, nvvk::DebugUtil& debug);
  void destroy();

  bool ensureResources(const TlasDescriptorInfo& tlasInfo, VkBuffer sensorBuffer,
                       VkBuffer sampleBuffer);
  void dispatch(const VkCommandBuffer& cmdBuf, uint32_t sensorIndex, uint32_t width, uint32_t height);

private:
  SensorGenerationPipeline m_pipeline;
};
