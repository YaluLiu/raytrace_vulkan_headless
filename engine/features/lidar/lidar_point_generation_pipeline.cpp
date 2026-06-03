#include "features/lidar/lidar_point_generation_pipeline.hpp"

#include "shaders/common/host_device.h"

void LidarPointGenerationPipeline::setup(VkDevice device, nvvk::DebugUtil& debug)
{
  m_pipeline.setup(device,
                   debug,
                   SensorGenerationPipelineConfig{"LidarPointGenerate",
                                                  "spv/lidar_points.comp.spv",
                                                  sizeof(PushConstantLidarGenerate)});
}

void LidarPointGenerationPipeline::destroy()
{
  m_pipeline.destroy();
}

bool LidarPointGenerationPipeline::ensureResources(const TlasDescriptorInfo& tlasInfo,
                                                   VkBuffer sensorBuffer,
                                                   VkBuffer pointBuffer)
{
  return m_pipeline.ensureResources(tlasInfo, sensorBuffer, pointBuffer);
}

void LidarPointGenerationPipeline::dispatch(const VkCommandBuffer& cmdBuf,
                                            uint32_t sensorIndex,
                                            uint32_t width,
                                            uint32_t height)
{
  PushConstantLidarGenerate pushConstant{};
  pushConstant.sensorIndex = sensorIndex;
  m_pipeline.dispatch(cmdBuf, &pushConstant, width, height);
}
