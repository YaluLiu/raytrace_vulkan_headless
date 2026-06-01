#include "features/height_scan/height_scan_pass.hpp"

#include "nvvk/commands_vk.hpp"
#include "features/preview/preview_raster_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
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
  m_sensors.clear();
  m_gpuSensors.clear();
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

void HeightScanPass::setSensors(std::vector<RasterHeightScanSensorSpec> sensors)
{
  m_sensors = std::move(sensors);
  m_layout = BuildHeightScanLayout(m_sensors);
  m_gpuSensors = buildGpuSensorMetadata();
  m_frameGenerated = false;
  if(m_layout.empty() || m_gpuSensors.size() != m_layout.sensors.size())
  {
    m_layout = {};
    m_gpuSensors.clear();
    destroyBuffers();
  }
}

void HeightScanPass::setVisualizationConfig(RasterHeightScanVisualizationConfig config)
{
  m_visualizationConfig = config;
}

void HeightScanPass::rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                                    const PreviewRasterPipeline& previewPipeline)
{
  m_overlayPipeline.destroyGraphicsPipeline();
  (void)sceneDescriptorSetLayout;
  (void)previewPipeline;
}

RasterHeightScanFrame HeightScanPass::readHeightScanFrame()
{
  RasterHeightScanFrame frame;
  frame.frameId = m_frameId;
  if(!currentLayoutGenerated() || m_device == VK_NULL_HANDLE || m_allocator == nullptr)
  {
    return frame;
  }

  const VkDeviceSize sampleBytes =
      static_cast<VkDeviceSize>(m_layout.totalSampleCount * sizeof(HeightScanSampleGpu));
  nvvk::Buffer stagingBuffer =
      m_allocator->createBuffer(sampleBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if(stagingBuffer.buffer == VK_NULL_HANDLE)
  {
    return frame;
  }

  {
    nvvk::CommandPool commandPool(m_device, m_graphicsQueueIndex);
    VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
    VkBufferMemoryBarrier beforeCopy{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    beforeCopy.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    beforeCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    beforeCopy.buffer = m_sampleBuffer.buffer;
    beforeCopy.offset = 0;
    beforeCopy.size = sampleBytes;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &beforeCopy, 0, nullptr);

    VkBufferCopy copyRegion{0, 0, sampleBytes};
    vkCmdCopyBuffer(cmdBuf, m_sampleBuffer.buffer, stagingBuffer.buffer, 1, &copyRegion);
    commandPool.submitAndWait(cmdBuf);
  }

  const auto* gpuSamples = static_cast<const HeightScanSampleGpu*>(m_allocator->map(stagingBuffer));
  if(gpuSamples == nullptr)
  {
    m_allocator->destroy(stagingBuffer);
    return frame;
  }

  for(const RasterHeightScanSensorMetadata& metadata : m_layout.sensors)
  {
    RasterHeightScanSensorGrid grid;
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
          RasterHeightScanSample sample;
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
  m_allocator->unmap(stagingBuffer);
  m_allocator->destroy(stagingBuffer);
  return frame;
}

void HeightScanPass::recordGenerate(const VkCommandBuffer& cmdBuf,
                                    std::optional<RasterTlasDescriptorInfo> tlasInfo)
{
  ++m_frameId;
  if(m_layout.empty() || !tlasInfo.has_value() || tlasInfo->accelerationStructure == VK_NULL_HANDLE)
  {
    m_frameGenerated = false;
    return;
  }

  if(!ensureSampleCapacity(m_layout.totalSampleCount) || !ensureSensorMetadataBuffer())
  {
    m_frameGenerated = false;
    return;
  }
  uploadSensorMetadata();

  if(!m_generationPipeline.ensureResources(*tlasInfo, m_sensorBuffer.buffer, m_sampleBuffer.buffer))
  {
    m_frameGenerated = false;
    return;
  }

  bool generatedAnySensor = false;
  for(const RasterHeightScanSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.sensorIndex >= m_sensors.size() || metadata.uSampleCount == 0 || metadata.vSampleCount == 0)
    {
      continue;
    }

    m_generationPipeline.dispatch(cmdBuf, metadata.sensorIndex, metadata.uSampleCount, metadata.vSampleCount);
    generatedAnySensor = true;

    VkBufferMemoryBarrier sampleBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    sampleBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    sampleBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    sampleBarrier.buffer = m_sampleBuffer.buffer;
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
                                   const PreviewRasterPipeline& previewPipeline)
{
  if(!currentLayoutGenerated() || !m_visualizationConfig.enabled ||
     m_visualizationConfig.pointSizePixels <= 0.0f ||
     m_sampleBuffer.buffer == VK_NULL_HANDLE || m_sensorBuffer.buffer == VK_NULL_HANDLE)
  {
    return;
  }

  try
  {
    if(!m_overlayPipeline.ensureResources(sceneDescriptorSetLayout, previewPipeline, m_sensorBuffer.buffer,
                                          m_sampleBuffer.buffer))
    {
      return;
    }
  }
  catch(const std::exception&)
  {
    return;
  }

  if(m_visualizationConfig.visualizeAllSensors)
  {
    for(const RasterHeightScanSensorMetadata& metadata : m_layout.sensors)
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

  const RasterHeightScanSensorMetadata& metadata = m_layout.sensors[m_visualizationConfig.sensorIndex];
  m_overlayPipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, m_visualizationConfig.sensorIndex,
                         m_visualizationConfig.pointSizePixels,
                         static_cast<uint32_t>(std::min<uint64_t>(metadata.sampleCount,
                                                                  std::numeric_limits<uint32_t>::max())));
}

bool HeightScanPass::ensureSampleCapacity(uint64_t sampleCount)
{
  if(sampleCount == 0 ||
     sampleCount >
         static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / sizeof(HeightScanSampleGpu)) ||
     m_allocator == nullptr)
  {
    return false;
  }
  if(m_sampleBuffer.buffer != VK_NULL_HANDLE && sampleCount <= m_sampleCapacity)
  {
    return true;
  }

  m_allocator->destroy(m_sampleBuffer);
  const VkDeviceSize sampleBytes = static_cast<VkDeviceSize>(sampleCount * sizeof(HeightScanSampleGpu));
  m_sampleBuffer =
      m_allocator->createBuffer(sampleBytes,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_sampleCapacity = m_sampleBuffer.buffer != VK_NULL_HANDLE ? sampleCount : 0;
  if(m_debug != nullptr && m_sampleBuffer.buffer != VK_NULL_HANDLE)
  {
    m_debug->setObjectName(m_sampleBuffer.buffer, "HeightScanSampleBuffer");
  }
  return m_sampleBuffer.buffer != VK_NULL_HANDLE;
}

bool HeightScanPass::ensureSensorMetadataBuffer()
{
  if(m_gpuSensors.empty() || m_allocator == nullptr)
  {
    return false;
  }
  const uint32_t sensorCount = static_cast<uint32_t>(m_gpuSensors.size());
  if(m_sensorBuffer.buffer != VK_NULL_HANDLE && sensorCount <= m_sensorCapacity)
  {
    return true;
  }

  m_allocator->destroy(m_sensorBuffer);
  const VkDeviceSize sensorBytes = static_cast<VkDeviceSize>(sensorCount * sizeof(HeightScanSensorGpu));
  m_sensorBuffer =
      m_allocator->createBuffer(sensorBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_sensorCapacity = m_sensorBuffer.buffer != VK_NULL_HANDLE ? sensorCount : 0;
  if(m_debug != nullptr && m_sensorBuffer.buffer != VK_NULL_HANDLE)
  {
    m_debug->setObjectName(m_sensorBuffer.buffer, "HeightScanSensorMetadataBuffer");
  }
  return m_sensorBuffer.buffer != VK_NULL_HANDLE;
}

void HeightScanPass::uploadSensorMetadata()
{
  if(m_sensorBuffer.buffer == VK_NULL_HANDLE || m_gpuSensors.empty() || m_allocator == nullptr)
  {
    return;
  }
  void* mapped = m_allocator->map(m_sensorBuffer);
  if(mapped == nullptr)
  {
    return;
  }
  std::memcpy(mapped, m_gpuSensors.data(), m_gpuSensors.size() * sizeof(HeightScanSensorGpu));
  m_allocator->unmap(m_sensorBuffer);
}

std::vector<HeightScanSensorGpu> HeightScanPass::buildGpuSensorMetadata() const
{
  std::vector<HeightScanSensorGpu> result;
  result.reserve(m_layout.sensors.size());
  for(const RasterHeightScanSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.sensorIndex >= m_sensors.size() ||
       metadata.sampleOffset > std::numeric_limits<uint32_t>::max() ||
       metadata.sampleCount > std::numeric_limits<uint32_t>::max() ||
       metadata.sampleOffset + metadata.sampleCount >
           static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1ull)
    {
      return {};
    }

    const RasterHeightScanSensorSpec& sensor = m_sensors[metadata.sensorIndex];
    const RasterHeightScanBasis basis = BuildHeightScanBasis(sensor);
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
  if(m_allocator != nullptr)
  {
    m_allocator->destroy(m_sampleBuffer);
    m_allocator->destroy(m_sensorBuffer);
  }
  m_sampleBuffer = {};
  m_sensorBuffer = {};
  m_sampleCapacity = 0;
  m_sensorCapacity = 0;
  m_frameGenerated = false;
}

bool HeightScanPass::currentLayoutGenerated() const
{
  return m_frameGenerated && !m_layout.empty() && m_gpuSensors.size() == m_layout.sensors.size() &&
         m_sampleBuffer.buffer != VK_NULL_HANDLE && m_sensorBuffer.buffer != VK_NULL_HANDLE &&
         m_sampleCapacity >= m_layout.totalSampleCount && m_sensorCapacity >= m_gpuSensors.size();
}
