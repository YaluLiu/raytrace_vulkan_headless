#include "features/height_scan/height_scan_pass.hpp"

#include "features/preview/preview_pipeline.hpp"
#include "features/sensor_common/sensor_readback.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace
{
constexpr float kHeightScanParamEpsilon = 1.0e-4f;

float sanitizeStep(float step)
{
  if(!std::isfinite(step))
  {
    return kHeightScanParamEpsilon;
  }
  return std::max(std::fabs(step), kHeightScanParamEpsilon);
}

float sanitizeRangeValue(float value, float other)
{
  return std::isfinite(value) && std::isfinite(other) ? value : 0.0f;
}
} // namespace

void HeightScanPass::setup(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsQueueIndex,
                           nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug)
{
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_allocator = &allocator;
  m_debug = &debug;
  m_buffers.setup(allocator, debug);
  (void)physicalDevice;
  m_generationPipeline.setup(device, debug);
  m_overlayPipeline.setup(device, debug,
                          PointOverlayPipelineConfig{"HeightScanOverlay", "spv/height_scan_overlay.vert.spv",
                                                     "spv/height_scan_overlay.frag.spv",
                                                     "height scan overlay"});
}

void HeightScanPass::destroy()
{
  m_overlayPipeline.destroy();
  m_generationPipeline.destroy();
  destroyBuffers();
  m_configuredSensors.clear();
  m_gpuSensorMetadata.clear();
  m_layout = {};
  m_device = VK_NULL_HANDLE;
  m_physicalDevice = VK_NULL_HANDLE;
  m_graphicsQueueIndex = 0;
  m_allocator = nullptr;
  m_debug = nullptr;
}

void HeightScanPass::destroyGraphicsPipeline()
{
  m_overlayPipeline.destroyGraphicsPipeline();
}

void HeightScanPass::configure(HeightScanOutputConfig config)
{
  m_visualizationConfig = config.visualization;
  m_configuredSensors = std::move(config.sensors);
  m_layout = BuildHeightScanLayout(m_configuredSensors);
  m_gpuSensorMetadata = buildGpuSensorMetadata();
  m_frameGenerated = false;
  if(m_layout.empty() || m_gpuSensorMetadata.size() != m_layout.sensors.size())
  {
    m_layout = {};
    m_gpuSensorMetadata.clear();
    destroyBuffers();
  }
}

void HeightScanPass::rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                                    const PreviewPipeline& previewPipeline)
{
  m_overlayPipeline.destroyGraphicsPipeline();
  (void)sceneDescriptorSetLayout;
  (void)previewPipeline;
}

HeightScanFrame HeightScanPass::readHeightScanFrame()
{
  HeightScanFrame frame;
  frame.frameId = m_frameId;
  if(!currentLayoutGenerated() || m_device == VK_NULL_HANDLE || m_allocator == nullptr)
  {
    return frame;
  }

  const VkDeviceSize sampleBytes =
      static_cast<VkDeviceSize>(m_layout.totalSampleCount * sizeof(HeightScanSampleGpu));
  const bool readbackOk =
      ReadDeviceBuffer(m_device,
                       m_graphicsQueueIndex,
                       *m_allocator,
                       m_buffers.generatedSamplesBuffer().buffer,
                       sampleBytes,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       [&](const void* mapped) {
                         const auto* gpuSamples = static_cast<const HeightScanSampleGpu*>(mapped);
                         for(const HeightScanSensorMetadata& metadata : m_layout.sensors)
                         {
                           HeightScanSensorGrid grid;
                           grid.name = metadata.name;
                           grid.sensorIndex = metadata.sensorIndex;
                           grid.width = metadata.uSampleCount;
                           grid.height = metadata.vSampleCount;
                           if(metadata.sampleOffset <= static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
                           {
                             const size_t begin = static_cast<size_t>(metadata.sampleOffset);
                             if(metadata.sampleCount <= static_cast<uint64_t>(std::numeric_limits<size_t>::max() - begin))
                             {
                               const size_t count = static_cast<size_t>(metadata.sampleCount);
                               grid.samples.reserve(count);
                               for(size_t index = 0; index < count; ++index)
                               {
                                 const HeightScanSampleGpu& gpuSample = gpuSamples[begin + index];
                                 HeightScanSample sample;
                                 sample.positionWs = glm::vec3(gpuSample.positionDistance);
                                 sample.distanceMeters = gpuSample.positionDistance.w;
                                 sample.sensorIndex = gpuSample.sensorIndex;
                                 sample.uIndex = gpuSample.uIndex;
                                 sample.vIndex = gpuSample.vIndex;
                                 sample.flags = gpuSample.flags;
                                 grid.samples.push_back(sample);
                               }
                             }
                           }
                           frame.sensors.push_back(std::move(grid));
                         }
                       });
  if(!readbackOk)
  {
    frame.sensors.clear();
    return frame;
  }
  return frame;
}

void HeightScanPass::recordGenerate(const VkCommandBuffer& cmdBuf,
                                    std::optional<TlasDescriptorInfo> tlasInfo)
{
  ++m_frameId;
  if(m_layout.empty() || !tlasInfo.has_value() || tlasInfo->accelerationStructure == VK_NULL_HANDLE)
  {
    m_frameGenerated = false;
    return;
  }

  if(m_gpuSensorMetadata.size() > std::numeric_limits<uint32_t>::max() ||
     !m_buffers.ensureGeneratedSampleCapacity(m_layout.totalSampleCount,
                                              sizeof(HeightScanSampleGpu),
                                              "HeightScanSampleBuffer") ||
     !m_buffers.ensureSensorMetadataCapacity(static_cast<uint32_t>(m_gpuSensorMetadata.size()),
                                             sizeof(HeightScanSensorGpu),
                                             "HeightScanSensorMetadataBuffer"))
  {
    m_frameGenerated = false;
    return;
  }
  uploadSensorMetadataToGpu();

  if(!m_generationPipeline.ensureResources(*tlasInfo,
                                           m_buffers.sensorMetadataBuffer().buffer,
                                           m_buffers.generatedSamplesBuffer().buffer))
  {
    m_frameGenerated = false;
    return;
  }

  bool generatedAnySensor = false;
  for(const HeightScanSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.sensorIndex >= m_configuredSensors.size() || metadata.uSampleCount == 0 ||
       metadata.vSampleCount == 0)
    {
      continue;
    }

    m_generationPipeline.dispatch(cmdBuf, metadata.sensorIndex, metadata.uSampleCount, metadata.vSampleCount);
    generatedAnySensor = true;

    VkBufferMemoryBarrier sampleBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    sampleBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    sampleBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    sampleBarrier.buffer = m_buffers.generatedSamplesBuffer().buffer;
    sampleBarrier.offset = metadata.sampleOffset * sizeof(HeightScanSampleGpu);
    sampleBarrier.size = metadata.sampleCount * sizeof(HeightScanSampleGpu);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &sampleBarrier, 0, nullptr);
  }
  m_frameGenerated = generatedAnySensor;
}

void HeightScanPass::recordOverlay(const VkCommandBuffer& cmdBuf,
                                   VkDescriptorSet sceneDescriptorSet,
                                   VkDescriptorSetLayout sceneDescriptorSetLayout,
                                   const PreviewPipeline& previewPipeline)
{
  if(!currentLayoutGenerated() || !m_visualizationConfig.enabled ||
     m_visualizationConfig.pointSizePixels <= 0.0f ||
     m_buffers.generatedSamplesBuffer().buffer == VK_NULL_HANDLE ||
     m_buffers.sensorMetadataBuffer().buffer == VK_NULL_HANDLE)
  {
    return;
  }

  if(!m_overlayPipeline.ensureResources(sceneDescriptorSetLayout,
                                        previewPipeline,
                                        m_buffers.sensorMetadataBuffer().buffer,
                                        m_buffers.generatedSamplesBuffer().buffer))
  {
    return;
  }

  if(m_visualizationConfig.visualizeAllSensors)
  {
    for(const HeightScanSensorMetadata& metadata : m_layout.sensors)
    {
      if(metadata.sensorIndex >= m_layout.sensors.size())
      {
        continue;
      }
      m_overlayPipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, metadata.sensorIndex,
                             m_visualizationConfig.pointSizePixels,
                             static_cast<uint32_t>(std::min<uint64_t>(metadata.sampleCount,
                                                                      std::numeric_limits<uint32_t>::max())));
    }
    return;
  }

  if(m_visualizationConfig.sensorIndex >= m_layout.sensors.size())
  {
    return;
  }

  const HeightScanSensorMetadata& metadata = m_layout.sensors[m_visualizationConfig.sensorIndex];
  m_overlayPipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, m_visualizationConfig.sensorIndex,
                         m_visualizationConfig.pointSizePixels,
                         static_cast<uint32_t>(std::min<uint64_t>(metadata.sampleCount,
                                                                  std::numeric_limits<uint32_t>::max())));
}

void HeightScanPass::uploadSensorMetadataToGpu()
{
  if(m_buffers.sensorMetadataBuffer().buffer == VK_NULL_HANDLE || m_gpuSensorMetadata.empty() ||
     m_allocator == nullptr)
  {
    return;
  }
  void* mapped = m_allocator->map(m_buffers.sensorMetadataBuffer());
  if(mapped == nullptr)
  {
    return;
  }
  std::memcpy(mapped, m_gpuSensorMetadata.data(),
              m_gpuSensorMetadata.size() * sizeof(HeightScanSensorGpu));
  m_allocator->unmap(m_buffers.sensorMetadataBuffer());
}

std::vector<HeightScanSensorGpu> HeightScanPass::buildGpuSensorMetadata() const
{
  std::vector<HeightScanSensorGpu> result;
  result.reserve(m_layout.sensors.size());
  for(const HeightScanSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.sensorIndex >= m_configuredSensors.size() ||
       metadata.sampleOffset > std::numeric_limits<uint32_t>::max() ||
       metadata.sampleCount > std::numeric_limits<uint32_t>::max() ||
       metadata.sampleOffset + metadata.sampleCount >
           static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1ull)
    {
      return {};
    }

    const HeightScanSensorSpec& sensor = m_configuredSensors[metadata.sensorIndex];
    const HeightScanBasis basis = BuildHeightScanBasis(sensor);
    const float uStart = sanitizeRangeValue(sensor.params.uStart, sensor.params.uEnd);
    const float uEnd = sanitizeRangeValue(sensor.params.uEnd, sensor.params.uStart);
    const float vStart = sanitizeRangeValue(sensor.params.vStart, sensor.params.vEnd);
    const float vEnd = sanitizeRangeValue(sensor.params.vEnd, sensor.params.vStart);

    HeightScanSensorGpu gpu{};
    gpu.originMaxRange = vec4(basis.origin, metadata.maxRange);
    gpu.gravityDirectionWsUStart = vec4(basis.gravityDirectionWs, uStart);
    gpu.axisUAndUEnd = vec4(basis.axisU, uEnd);
    gpu.axisVAndVStart = vec4(basis.axisV, vStart);
    gpu.uStepVEndVStep =
        vec4(sanitizeStep(sensor.params.uStep), vEnd, sanitizeStep(sensor.params.vStep), 0.0f);
    gpu.sampleOffset = static_cast<uint32_t>(metadata.sampleOffset);
    gpu.sampleCount = static_cast<uint32_t>(metadata.sampleCount);
    gpu.uSampleCount = metadata.uSampleCount;
    gpu.vSampleCount = metadata.vSampleCount;
    result.push_back(gpu);
  }
  return result;
}

void HeightScanPass::destroyBuffers()
{
  m_buffers.destroy();
  m_frameGenerated = false;
}

bool HeightScanPass::currentLayoutGenerated() const
{
  return m_frameGenerated && !m_layout.empty() &&
         m_gpuSensorMetadata.size() == m_layout.sensors.size() &&
         m_buffers.generatedSamplesBuffer().buffer != VK_NULL_HANDLE &&
         m_buffers.sensorMetadataBuffer().buffer != VK_NULL_HANDLE &&
         m_buffers.generatedSampleCapacity() >= m_layout.totalSampleCount &&
         m_buffers.sensorMetadataCapacity() >= m_gpuSensorMetadata.size();
}
