#pragma once

#include <raster/height_scan_types.hpp>
#include <raster/raster_renderer_types.hpp>

#include "features/height_scan/height_scan.hpp"
#include "features/height_scan/height_scan_generation_pipeline.hpp"
#include "features/point_overlay/point_overlay_pipeline.hpp"
#include "shaders/common/host_device.h"

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <optional>
#include <vector>

#include <vulkan/vulkan_core.h>

class PreviewRasterPipeline;

class HeightScanPass final
{
public:
  void setup(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  void setSensors(std::vector<RasterHeightScanSensorSpec> sensors);
  void setVisualizationConfig(RasterHeightScanVisualizationConfig config);
  const RasterHeightScanLayout& getCurrentLayout() const { return m_layout; }
  RasterHeightScanFrame readHeightScanFrame();

  void rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                      const PreviewRasterPipeline& previewPipeline);
  void recordGenerate(const VkCommandBuffer& cmdBuf, std::optional<RasterTlasDescriptorInfo> tlasInfo);
  void recordOverlay(const VkCommandBuffer& cmdBuf,
                     VkDescriptorSet sceneDescriptorSet,
                     VkDescriptorSetLayout sceneDescriptorSetLayout,
                     const PreviewRasterPipeline& previewPipeline);

private:
  bool ensureSampleCapacity(uint64_t sampleCount);
  bool ensureSensorMetadataBuffer();
  void uploadSensorMetadata();
  std::vector<HeightScanSensorGpu> buildGpuSensorMetadata() const;
  void destroyBuffers();
  bool currentLayoutGenerated() const;

  VkDevice m_device{VK_NULL_HANDLE};
  VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  std::vector<RasterHeightScanSensorSpec> m_sensors;
  RasterHeightScanLayout m_layout;
  uint64_t m_sampleCapacity{0};
  uint32_t m_sensorCapacity{0};
  uint64_t m_frameId{0};
  bool m_frameGenerated{false};
  RasterHeightScanVisualizationConfig m_visualizationConfig;

  nvvk::Buffer m_sampleBuffer;
  nvvk::Buffer m_sensorBuffer;
  std::vector<HeightScanSensorGpu> m_gpuSensors;

  HeightScanGenerationPipeline m_generationPipeline;
  PointOverlayPipeline m_overlayPipeline;
};
