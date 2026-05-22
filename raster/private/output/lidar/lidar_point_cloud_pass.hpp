#pragma once

#include <raster/lidar_types.hpp>
#include <raster/raster_renderer_types.hpp>

#include "output/lidar/lidar_point_generation_pipeline.hpp"
#include "output/lidar/lidar_point_overlay_pipeline.hpp"
#include "output/lidar/lidar_scan.hpp"
#include "scene/raster_scene_types.hpp"
#include "shaders/common/host_device.h"

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <optional>
#include <span>

#include <vulkan/vulkan_core.h>

class PreviewRasterPipeline;

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

  void setSensors(std::vector<RasterLidarSensorSpec> sensors);
  void setVisualizationConfig(RasterLidarVisualizationConfig config);
  const RasterLidarScanLayout& getCurrentLayout() const { return m_layout; }
  RasterLidarFramePointCloud readPointCloudFrame();

  void rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                      const PreviewRasterPipeline& previewPipeline);
  void recordGenerate(const VkCommandBuffer& cmdBuf,
                      std::optional<RasterTlasDescriptorInfo> tlasInfo);
  void recordOverlay(const VkCommandBuffer& cmdBuf,
                     VkDescriptorSet sceneDescriptorSet,
                     VkDescriptorSetLayout sceneDescriptorSetLayout,
                     const PreviewRasterPipeline& previewPipeline);

private:
  bool ensurePointCapacity(uint64_t pointCount);
  bool ensureSensorMetadataBuffer();
  void uploadSensorMetadata();
  std::vector<LidarSensorGpu> buildGpuSensorMetadata() const;
  void destroyBuffers();
  bool currentLayoutGenerated() const;

  VkDevice m_device{VK_NULL_HANDLE};
  VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  std::vector<RasterLidarSensorSpec> m_sensors;
  RasterLidarVisualizationConfig m_visualization;
  RasterLidarScanLayout m_layout;
  uint64_t m_pointCapacity{0};
  uint32_t m_sensorCapacity{0};
  uint64_t m_frameId{0};
  bool m_frameGenerated{false};

  nvvk::Buffer m_pointBuffer;
  nvvk::Buffer m_sensorBuffer;
  std::vector<LidarSensorGpu> m_gpuSensors;

  LidarPointGenerationPipeline m_generationPipeline;
  LidarPointOverlayPipeline m_overlayPipeline;
};
