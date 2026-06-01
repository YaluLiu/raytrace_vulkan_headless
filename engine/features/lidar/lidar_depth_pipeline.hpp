#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "features/lidar/lidar_scan.hpp"
#include "scene/scene_types.hpp"

#include <span>

#include <vulkan/vulkan_core.h>

class LidarDepthPipeline final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  bool ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout, VkExtent2D maxExtent);
  bool draw(const VkCommandBuffer& cmdBuf,
                 const LidarSensorSpec& sensor,
                 const LidarSensorMetadata& metadata,
                 VkDescriptorSet sceneDescriptorSet,
                 std::span<const MeshBuffers> objModels,
                 std::span<const SceneInstance> instances,
                 std::span<const int> instanceIds);

  const nvvk::Texture& getRangeTexture() const { return m_rangeImage; }
  VkExtent2D getExtent() const { return m_extent; }

private:
  void createImages(VkExtent2D extent);
  void createRenderPass();
  void createFramebuffer();
  void createGraphicsPipeline();
  void destroyImages();
  void destroyFramebuffer();

  VkDevice m_device{VK_NULL_HANDLE};
  VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  VkDescriptorSetLayout m_sceneDescriptorSetLayout{VK_NULL_HANDLE};
  VkExtent2D m_extent{0, 0};
  VkFormat m_rangeFormat{VK_FORMAT_R32_SFLOAT};
  VkFormat m_depthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  nvvk::Texture m_rangeImage;
  nvvk::Texture m_depthImage;
  VkRenderPass m_renderPass{VK_NULL_HANDLE};
  VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
};
