#include "features/lidar/lidar_point_cloud_pass.hpp"

#include "features/preview/preview_pipeline.hpp"
#include "features/sensor_common/sensor_readback.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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
  m_buffers.setup(allocator, debug);
  (void)physicalDevice;
  m_generationPipeline.setup(device, debug);
  m_overlayPipeline.setup(device, debug,
                          PointOverlayPipelineConfig{"LidarPointOverlay", "spv/lidar_overlay.vert.spv",
                                                     "spv/lidar_overlay.frag.spv", "LiDAR overlay"});
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

void LidarPointCloudPass::configure(LidarOutputConfig config)
{
  m_visualization = config.visualization;
  m_sensors = std::move(config.sensors);
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

LidarFramePointCloud LidarPointCloudPass::readPointCloudFrame()
{
  LidarFramePointCloud frame;
  frame.frameId = m_frameId;
  if(!currentLayoutGenerated() || m_device == VK_NULL_HANDLE || m_allocator == nullptr)
  {
    return frame;
  }

  const VkDeviceSize pointBytes = static_cast<VkDeviceSize>(m_layout.totalPointCount * sizeof(LidarPointGpu));
  const bool readbackOk =
      ReadDeviceBuffer(m_device,
                       m_graphicsQueueIndex,
                       *m_allocator,
                       m_buffers.outputBuffer().buffer,
                       pointBytes,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       [&](const void* mapped) {
                         const auto* gpuPoints = static_cast<const LidarPointGpu*>(mapped);
                         for(const LidarSensorMetadata& metadata : m_layout.sensors)
                         {
                           LidarSensorPointCloud cloud;
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
                               LidarPoint point;
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
                       });
  if(!readbackOk)
  {
    frame.sensors.clear();
    return frame;
  }
  return frame;
}

void LidarPointCloudPass::rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                                         const PreviewPipeline& previewPipeline)
{
  m_overlayPipeline.destroyGraphicsPipeline();
  (void)sceneDescriptorSetLayout;
  (void)previewPipeline;
}

void LidarPointCloudPass::recordGenerate(const VkCommandBuffer& cmdBuf,
                                         std::optional<TlasDescriptorInfo> tlasInfo)
{
  ++m_frameId;
  if(m_layout.empty() || !tlasInfo.has_value() || tlasInfo->accelerationStructure == VK_NULL_HANDLE)
  {
    m_frameGenerated = false;
    return;
  }

  if(m_gpuSensors.size() > std::numeric_limits<uint32_t>::max() ||
     !m_buffers.ensureOutputCapacity(m_layout.totalPointCount, sizeof(LidarPointGpu), "LidarPointBuffer") ||
     !m_buffers.ensureSensorCapacity(static_cast<uint32_t>(m_gpuSensors.size()),
                                     sizeof(LidarSensorGpu),
                                     "LidarSensorMetadataBuffer"))
  {
    m_frameGenerated = false;
    return;
  }
  uploadSensorMetadata();

  if(!m_generationPipeline.ensureResources(*tlasInfo, m_buffers.sensorBuffer().buffer,
                                           m_buffers.outputBuffer().buffer))
  {
    m_frameGenerated = false;
    return;
  }

  bool generatedAnySensor = false;
  for(const LidarSensorMetadata& metadata : m_layout.sensors)
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
    pointBarrier.buffer = m_buffers.outputBuffer().buffer;
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
                                        const PreviewPipeline& previewPipeline)
{
  if(!currentLayoutGenerated() || !m_visualization.enabled || m_visualization.pointSizePixels <= 0.0f)
  {
    return;
  }

  if(!m_overlayPipeline.ensureResources(sceneDescriptorSetLayout, previewPipeline, m_buffers.sensorBuffer().buffer,
                                        m_buffers.outputBuffer().buffer))
  {
    return;
  }

  if(m_visualization.visualizeAllSensors)
  {
    for(const LidarSensorMetadata& metadata : m_layout.sensors)
    {
      if(metadata.sensorIndex >= m_layout.sensors.size())
      {
        continue;
      }
      m_overlayPipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, metadata.sensorIndex,
                             m_visualization.pointSizePixels,
                             static_cast<uint32_t>(std::min<uint64_t>(metadata.pointCount,
                                                                      std::numeric_limits<uint32_t>::max())));
    }
    return;
  }

  if(m_visualization.sensorIndex >= m_layout.sensors.size())
  {
    return;
  }

  const LidarSensorMetadata& metadata = m_layout.sensors[m_visualization.sensorIndex];
  m_overlayPipeline.draw(cmdBuf, previewPipeline, sceneDescriptorSet, m_visualization.sensorIndex,
                         m_visualization.pointSizePixels,
                         static_cast<uint32_t>(std::min<uint64_t>(metadata.pointCount,
                                                                  std::numeric_limits<uint32_t>::max())));
}

void LidarPointCloudPass::uploadSensorMetadata()
{
  if(m_buffers.sensorBuffer().buffer == VK_NULL_HANDLE || m_gpuSensors.empty() || m_allocator == nullptr)
  {
    return;
  }
  void* mapped = m_allocator->map(m_buffers.sensorBuffer());
  if(mapped == nullptr)
  {
    return;
  }
  std::memcpy(mapped, m_gpuSensors.data(), m_gpuSensors.size() * sizeof(LidarSensorGpu));
  m_allocator->unmap(m_buffers.sensorBuffer());
}

std::vector<LidarSensorGpu> LidarPointCloudPass::buildGpuSensorMetadata() const
{
  std::vector<LidarSensorGpu> result;
  result.reserve(m_layout.sensors.size());
  for(const LidarSensorMetadata& metadata : m_layout.sensors)
  {
    if(metadata.pointOffset > std::numeric_limits<uint32_t>::max() ||
       metadata.pointCount > std::numeric_limits<uint32_t>::max() ||
       metadata.pointOffset + metadata.pointCount >
           static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1ull)
    {
      return {};
    }

    const LidarSensorSpec& sensor = m_sensors[metadata.sensorIndex];
    const LidarBasis basis = BuildLidarBasis(sensor);

    LidarSensorGpu gpu{};
    gpu.originMaxRange = vec4(basis.origin, metadata.maxRange);
    gpu.forwardAzimuthStart = vec4(basis.forward, sensor.params.azimuthStartDeg);
    gpu.rightAzimuthStep = vec4(basis.right, std::max(std::fabs(sensor.params.azimuthStepDeg), 1.0e-4f));
    gpu.upVerticalStart = vec4(basis.up, sensor.params.verticalStartDeg);
    gpu.azimuthEndVerticalEndStep = vec4(sensor.params.azimuthEndDeg, sensor.params.verticalEndDeg,
                                         std::max(std::fabs(sensor.params.verticalStepDeg), 1.0e-4f), 0.0f);
    gpu.pointOffset = static_cast<uint32_t>(metadata.pointOffset);
    gpu.pointCount = static_cast<uint32_t>(metadata.pointCount);
    gpu.azimuthSampleCount = metadata.azimuthSampleCount;
    gpu.verticalSampleCount = metadata.verticalSampleCount;
    gpu.intensity = std::max(sensor.params.intensity, 0.0f);
    result.push_back(gpu);
  }
  return result;
}

void LidarPointCloudPass::destroyBuffers()
{
  m_buffers.destroy();
  m_frameGenerated = false;
}

bool LidarPointCloudPass::currentLayoutGenerated() const
{
  return m_frameGenerated && !m_layout.empty() && m_gpuSensors.size() == m_layout.sensors.size() &&
         m_buffers.outputBuffer().buffer != VK_NULL_HANDLE && m_buffers.sensorBuffer().buffer != VK_NULL_HANDLE &&
         m_buffers.outputCapacity() >= m_layout.totalPointCount && m_buffers.sensorCapacity() >= m_gpuSensors.size();
}
