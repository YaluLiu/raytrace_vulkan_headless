#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan_core.h>

#include "nvvk/debug_util_vk.hpp"
#include "output/preview_raster_pipeline.hpp"
#include "scene/raster_scene_types.hpp"

class MultiviewTileRasterPipeline final
{
public:
  void setup(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsQueueIndex, nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  bool ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                       const PreviewRasterPipeline& referencePipeline,
                       uint32_t viewCount);

  VkRenderPass getRenderPass() const { return _renderPass; }
  uint32_t getViewCount() const { return _viewCount; }

  bool rasterize(const VkCommandBuffer& cmdBuf,
                 VkFramebuffer framebuffer,
                 VkExtent2D tileExtent,
                 VkDescriptorSet sceneDescriptorSet,
                 std::span<const RasterMeshBuffers> objModels,
                 std::span<const RasterInstance> instances,
                 std::span<const int> instanceIds);

private:
  void createRenderPass();
  void createGraphicsPipeline();

  VkDevice _device{VK_NULL_HANDLE};
  VkPhysicalDevice _physicalDevice{VK_NULL_HANDLE};
  uint32_t _graphicsQueueIndex{0};
  nvvk::DebugUtil* _debug{nullptr};
  VkDescriptorSetLayout _sceneDescriptorSetLayout{VK_NULL_HANDLE};

  VkFormat _colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  VkFormat _depthAovFormat{VK_FORMAT_R32_SFLOAT};
  VkFormat _depthAttachmentFormat{VK_FORMAT_X8_D24_UNORM_PACK32};
  uint32_t _viewCount{0};

  VkRenderPass _renderPass{VK_NULL_HANDLE};
  VkPipeline _rasterPipeline{VK_NULL_HANDLE};
  VkPipeline _domeBackgroundPipeline{VK_NULL_HANDLE};
  VkPipelineLayout _pipelineLayout{VK_NULL_HANDLE};
};
