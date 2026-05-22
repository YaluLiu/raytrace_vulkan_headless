#include "output/lidar/lidar_point_cloud_pass.hpp"

#include "nvvk/commands_vk.hpp"
#include "output/preview_raster_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

void LidarPointCloudPass::setup(VkDevice device,
                                VkPhysicalDevice physicalDevice,
                                uint32_t graphicsQueueIndex,
                                nvvk::ResourceAllocatorDma& allocator,
                                nvvk::DebugUtil& debug)
{
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_allocator = &allocator;
  m_debug = &debug;
  (void)physicalDevice;
  m_generationPipeline.setup(device, debug);
  m_overlayPipeline.setup(device, debug);
}

void LidarPointCloudPass::destroy()
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

void LidarPointCloudPass::destroyGraphicsPipeline()
{
  m_overlayPipeline.destroyGraphicsPipeline();
}

void LidarPointCloudPass::setSensors(std::vector<RasterLidarSensorSpec> sensors)
{
  m_sensors = std::move(sensors);
  m_layout = BuildLidarScanLayout(m_sensors);
  m_gpuSensors = buildGpuSensorMetadata();
  m_frameGenerated = false;
  if(m_layout.empty() || m_gpuSensors.size() != m_layout.sensors.size())
  {
    m_layout = {};
    m_gpuSensors.clear();
    destroyBuffers();
  }
}

void LidarPointCloudPass::setVisualizationConfig(RasterLidarVisualizationConfig config)
{
  m_visualization = config;
}

RasterLidarFramePointCloud LidarPointCloudPass::readPointCloudFrame()
{
  RasterLidarFramePointCloud frame;
  frame.frameId = m_frameId;
  if(!currentLayoutGenerated() || m_device == VK_NULL_HANDLE || m_allocator == nullptr)
  {
    return frame;
  }

  const VkDeviceSize pointBytes = static_cast<VkDeviceSize>(m_layout.totalPointCount * sizeof(LidarPointGpu));
  nvvk::Buffer stagingBuffer =
      m_allocator->createBuffer(pointBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
    beforeCopy.buffer = m_pointBuffer.buffer;
    beforeCopy.offset = 0;
    beforeCopy.size = pointBytes;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &beforeCopy, 0, nullptr);

    VkBufferCopy copyRegion{0, 0, pointBytes};
    vkCmdCopyBuffer(cmdBuf, m_pointBuffer.buffer, stagingBuffer.buffer, 1, &copyRegion);
    commandPool.submitAndWait(cmdBuf);
  }

  const auto* gpuPoints = static_cast<const LidarPointGpu*>(m_allocator->map(stagingBuffer));
  if(gpuPoints == nullptr)
  {
    m_allocator->destroy(stagingBuffer);
    return frame;
  }

  for(const RasterLidarSensorMetadata& metadata : m_layout.sensors)
  {
    RasterLidarSensorPointCloud cloud;
    cloud.name = metadata.name;
    cloud.sensorIndex = metadata.sensorIndex;
    cloud.width = metadata.azimuthSampleCount;
    cloud.height = metadata.verticalSampleCount;
    if(metadata.pointOffset <= static_cast<uint64_t>(std::numeric_limits<size_t>::max()) &&
       metadata.pointCount <= static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    {
      cloud.points.reserve(static_cast<size_t>(metadata.pointCount));
      const size_t begin = static_cast<size_t>(metadata.pointOffset);
      const size_t count = static_cast<size_t>(metadata.pointCount);
      for(size_t index = 0; index < count; ++index)
      {
        const LidarPointGpu& gpuPoint = gpuPoints[begin + index];
        RasterLidarPoint point;
        point.positionWs = glm::vec3(gpuPoint.positionRange);
        point.rangeMeters = gpuPoint.positionRange.w;
        point.sensorIndex = gpuPoint.sensorIndex;
        point.ringIndex = gpuPoint.ringIndex;
        point.beamIndex = gpuPoint.beamIndex;
        point.flags = gpuPoint.flags;
        point.intensity = gpuPoint.intensity;
        cloud.points.push_back(point);
      }
    }
    frame.sensors.push_back(std::move(cloud));
  }
  m_allocator->unmap(stagingBuffer);
  m_allocator->destroy(stagingBuffer);
  return frame;
}

void LidarPointCloudPass::rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                                         const PreviewRasterPipeline& previewPipeline)
{
  m_overlayPipeline.destroyGraphicsPipeline();
  (void)sceneDescriptorSetLayout;
  (void)previewPipeline;
}

void LidarPointCloudPass::recordGenerate(const VkCommandBuffer& cmdBuf,
                                         std::optional<RasterTlasDescriptorInfo> tlasInfo)
{
  ++m_frameId;
  if(m_layout.empty() || !tlasInfo.has_value() || tlasInfo->accelerationStructure == VK_NULL_HANDLE)
  {
    m_frameGenerated = false;
    return;
  }

  if(!ensurePointCapacity(m_layout.totalPointCount) || !ensureSensorMetadataBuffer())
  {
    m_frameGenerated = false;
    return;
  }
  uploadSensorMetadata();

  if(!m_generationPipeline.ensureResources(*tlasInfo, m_sensorBuffer.buffer, m_pointBuffer.buffer))
  {
    m_frameGenerated = false;
    return;
  }

  bool generatedAnySensor = false;
  for(const RasterLidarSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.sensorIndex >= m_sensors.size() || metadata.azimuthSampleCount == 0 ||
       metadata.verticalSampleCount == 0)
    {
      continue;
    }

    m_generationPipeline.dispatch(cmdBuf, metadata.sensorIndex, metadata.azimuthSampleCount,
                                  metadata.verticalSampleCount);
    generatedAnySensor = true;

    VkBufferMemoryBarrier pointBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    pointBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    pointBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    pointBarrier.buffer = m_pointBuffer.buffer;
    pointBarrier.offset = metadata.pointOffset * sizeof(LidarPointGpu);
    pointBarrier.size = metadata.pointCount * sizeof(LidarPointGpu);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &pointBarrier, 0, nullptr);
  }
  m_frameGenerated = generatedAnySensor;
}

void LidarPointCloudPass::recordOverlay(const VkCommandBuffer& cmdBuf,
                                        VkDescriptorSet sceneDescriptorSet,
                                        VkDescriptorSetLayout sceneDescriptorSetLayout,
                                        const PreviewRasterPipeline& previewPipeline)
{
  if(!currentLayoutGenerated() || !m_visualization.enabled || m_visualization.pointSizePixels <= 0.0f ||
     m_visualization.sensorIndex >= m_layout.sensors.size())
  {
    return;
  }

  if(!m_overlayPipeline.ensureResources(sceneDescriptorSetLayout, previewPipeline, m_sensorBuffer.buffer,
                                        m_pointBuffer.buffer))
  {
    return;
  }

  const RasterLidarSensorMetadata& metadata = m_layout.sensors[m_visualization.sensorIndex];
  m_overlayPipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, m_visualization,
                         static_cast<uint32_t>(std::min<uint64_t>(metadata.pointCount,
                                                                  std::numeric_limits<uint32_t>::max())));
}

bool LidarPointCloudPass::ensurePointCapacity(uint64_t pointCount)
{
  if(pointCount == 0 || pointCount > static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / sizeof(LidarPointGpu)) ||
     m_allocator == nullptr)
  {
    return false;
  }
  if(m_pointBuffer.buffer != VK_NULL_HANDLE && pointCount <= m_pointCapacity)
  {
    return true;
  }

  m_allocator->destroy(m_pointBuffer);
  const VkDeviceSize pointBytes = static_cast<VkDeviceSize>(pointCount * sizeof(LidarPointGpu));
  m_pointBuffer = m_allocator->createBuffer(pointBytes,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_pointCapacity = m_pointBuffer.buffer != VK_NULL_HANDLE ? pointCount : 0;
  if(m_debug != nullptr && m_pointBuffer.buffer != VK_NULL_HANDLE)
  {
    m_debug->setObjectName(m_pointBuffer.buffer, "LidarPointBuffer");
  }
  return m_pointBuffer.buffer != VK_NULL_HANDLE;
}

bool LidarPointCloudPass::ensureSensorMetadataBuffer()
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
  const VkDeviceSize sensorBytes = static_cast<VkDeviceSize>(sensorCount * sizeof(LidarSensorGpu));
  m_sensorBuffer = m_allocator->createBuffer(sensorBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_sensorCapacity = m_sensorBuffer.buffer != VK_NULL_HANDLE ? sensorCount : 0;
  if(m_debug != nullptr && m_sensorBuffer.buffer != VK_NULL_HANDLE)
  {
    m_debug->setObjectName(m_sensorBuffer.buffer, "LidarSensorMetadataBuffer");
  }
  return m_sensorBuffer.buffer != VK_NULL_HANDLE;
}

void LidarPointCloudPass::uploadSensorMetadata()
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
  std::memcpy(mapped, m_gpuSensors.data(), m_gpuSensors.size() * sizeof(LidarSensorGpu));
  m_allocator->unmap(m_sensorBuffer);
}

std::vector<LidarSensorGpu> LidarPointCloudPass::buildGpuSensorMetadata() const
{
  std::vector<LidarSensorGpu> result;
  result.reserve(m_layout.sensors.size());
  for(const RasterLidarSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.pointOffset > std::numeric_limits<uint32_t>::max() ||
       metadata.pointCount > std::numeric_limits<uint32_t>::max() ||
       metadata.pointOffset + metadata.pointCount >
           static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1ull)
    {
      return {};
    }

    const RasterLidarSensorSpec& sensor = m_sensors[metadata.sensorIndex];
    const RasterLidarBasis basis = BuildLidarBasis(sensor);

    LidarSensorGpu gpu{};
    gpu.originMaxDistance = vec4(basis.origin, metadata.maxDistance);
    gpu.forwardAzimuthMin = vec4(basis.forward, sensor.params.azimuthMinDeg);
    gpu.rightAzimuthStep = vec4(basis.right, std::max(std::fabs(sensor.params.azimuthStepDeg), 1.0e-4f));
    gpu.upVerticalMin = vec4(basis.up, sensor.params.verticalMinDeg);
    gpu.azimuthMaxVerticalMaxStep = vec4(sensor.params.azimuthMaxDeg, sensor.params.verticalMaxDeg,
                                         std::max(std::fabs(sensor.params.verticalStepDeg), 1.0e-4f), 0.0f);
    gpu.pointOffset = static_cast<uint32_t>(metadata.pointOffset);
    gpu.pointCount = static_cast<uint32_t>(metadata.pointCount);
    gpu.azimuthSampleCount = metadata.azimuthSampleCount;
    gpu.verticalSampleCount = metadata.verticalSampleCount;
    result.push_back(gpu);
  }
  return result;
}

void LidarPointCloudPass::destroyBuffers()
{
  if(m_allocator != nullptr)
  {
    m_allocator->destroy(m_pointBuffer);
    m_allocator->destroy(m_sensorBuffer);
  }
  m_pointBuffer = {};
  m_sensorBuffer = {};
  m_pointCapacity = 0;
  m_sensorCapacity = 0;
  m_frameGenerated = false;
}

bool LidarPointCloudPass::currentLayoutGenerated() const
{
  return m_frameGenerated && !m_layout.empty() && m_gpuSensors.size() == m_layout.sensors.size() &&
         m_pointBuffer.buffer != VK_NULL_HANDLE && m_sensorBuffer.buffer != VK_NULL_HANDLE &&
         m_pointCapacity >= m_layout.totalPointCount && m_sensorCapacity >= m_gpuSensors.size();
}
