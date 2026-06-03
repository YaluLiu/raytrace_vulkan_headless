#include "features/height_scan/height_scan_generation_pipeline.hpp"

#include "shaders/common/host_device.h"

void HeightScanGenerationPipeline::setup(VkDevice device, nvvk::DebugUtil& debug)
{
  m_pipeline.setup(device,
                   debug,
                   SensorGenerationPipelineConfig{"HeightScanGenerate",
                                                  "spv/height_scan.comp.spv",
                                                  sizeof(PushConstantHeightScanGenerate)});
}

void HeightScanGenerationPipeline::destroy()
{
  m_pipeline.destroy();
}

bool HeightScanGenerationPipeline::ensureResources(const TlasDescriptorInfo& tlasInfo,
                                                   VkBuffer sensorBuffer,
                                                   VkBuffer sampleBuffer)
{
  return m_pipeline.ensureResources(tlasInfo, sensorBuffer, sampleBuffer);
}

void HeightScanGenerationPipeline::dispatch(const VkCommandBuffer& cmdBuf,
                                            uint32_t sensorIndex,
                                            uint32_t width,
                                            uint32_t height)
{
  PushConstantHeightScanGenerate pushConstant{};
  pushConstant.sensorIndex = sensorIndex;
  m_pipeline.dispatch(cmdBuf, &pushConstant, width, height);
}
