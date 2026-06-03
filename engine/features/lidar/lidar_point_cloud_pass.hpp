#pragma once

#include <engine/output_config.hpp>
#include <engine/renderer_types.hpp>

#include "features/lidar/lidar_point_generation_pipeline.hpp"
#include "features/lidar/lidar_scan.hpp"
#include "features/point_overlay/point_overlay_pipeline.hpp"
#include "features/sensor_common/sensor_gpu_buffers.hpp"
#include "scene/scene_types.hpp"
#include "shaders/common/host_device.h"

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <optional>
#include <span>

#include <vulkan/vulkan_core.h>

class PreviewPipeline;

class LidarPointCloudPass final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  void configure(LidarOutputConfig config);
  const LidarScanLayout& getCurrentLayout() const { return m_layout; }
  LidarFramePointCloud readPointCloudFrame();

  void rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                      const PreviewPipeline& previewPipeline);
  void recordGenerate(const VkCommandBuffer& cmdBuf,
                      std::optional<TlasDescriptorInfo> tlasInfo);
  void recordOverlay(const VkCommandBuffer& cmdBuf,
                     VkDescriptorSet sceneDescriptorSet,
                     VkDescriptorSetLayout sceneDescriptorSetLayout,
                     const PreviewPipeline& previewPipeline);

private:
  void uploadSensorMetadata();
  std::vector<LidarSensorGpu> buildGpuSensorMetadata() const;
  void destroyBuffers();
  bool currentLayoutGenerated() const;

  VkDevice m_device{VK_NULL_HANDLE};
  VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  std::vector<LidarSensorSpec> m_sensors;
  LidarVisualizationConfig m_visualization;
  LidarScanLayout m_layout;
  uint64_t m_frameId{0};
  bool m_frameGenerated{false};

  SensorGpuBuffers m_buffers;
  std::vector<LidarSensorGpu> m_gpuSensors;

  LidarPointGenerationPipeline m_generationPipeline;
  PointOverlayPipeline m_overlayPipeline;
};
