#pragma once

#include <engine/height_scan_types.hpp>
#include <engine/renderer_types.hpp>

#include "features/height_scan/height_scan.hpp"
#include "features/height_scan/height_scan_generation_pipeline.hpp"
#include "features/point_overlay/point_overlay_pipeline.hpp"
#include "shaders/common/host_device.h"

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <optional>
#include <vector>

#include <vulkan/vulkan_core.h>

class PreviewPipeline;

class HeightScanPass final
{
public:
  void setup(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  void setSensors(std::vector<HeightScanSensorSpec> sensors);
  void setVisualizationConfig(HeightScanVisualizationConfig config);
  const HeightScanLayout& getCurrentLayout() const { return m_layout; }
  HeightScanFrame readHeightScanFrame();

  void rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                      const PreviewPipeline& previewPipeline);
  void recordGenerate(const VkCommandBuffer& cmdBuf, std::optional<TlasDescriptorInfo> tlasInfo);
  void recordOverlay(const VkCommandBuffer& cmdBuf,
                     VkDescriptorSet sceneDescriptorSet,
                     VkDescriptorSetLayout sceneDescriptorSetLayout,
                     const PreviewPipeline& previewPipeline);

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

  std::vector<HeightScanSensorSpec> m_sensors;
  HeightScanLayout m_layout;
  uint64_t m_sampleCapacity{0};
  uint32_t m_sensorCapacity{0};
  uint64_t m_frameId{0};
  bool m_frameGenerated{false};
  HeightScanVisualizationConfig m_visualizationConfig;

  nvvk::Buffer m_sampleBuffer;
  nvvk::Buffer m_sensorBuffer;
  std::vector<HeightScanSensorGpu> m_gpuSensors;

  HeightScanGenerationPipeline m_generationPipeline;
  PointOverlayPipeline m_overlayPipeline;
};
