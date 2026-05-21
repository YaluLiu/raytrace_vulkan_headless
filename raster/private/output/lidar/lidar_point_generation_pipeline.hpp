#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"

#include <vulkan/vulkan_core.h>

class LidarPointGenerationPipeline final
{
public:
  void setup(VkDevice device, nvvk::DebugUtil& debug);
  void destroy();

  bool ensureResources(VkImageView rangeImageView, VkBuffer sensorBuffer, VkBuffer pointBuffer);
  void dispatch(const VkCommandBuffer& cmdBuf, uint32_t sensorIndex, uint32_t width, uint32_t height);

private:
  void createDescriptorResources();
  void createPipeline();
  void updateDescriptorSet(VkImageView rangeImageView, VkBuffer sensorBuffer, VkBuffer pointBuffer);

  VkDevice m_device{VK_NULL_HANDLE};
  nvvk::DebugUtil* m_debug{nullptr};
  nvvk::DescriptorSetBindings m_bindings;
  VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkImageView m_boundRangeImageView{VK_NULL_HANDLE};
  VkBuffer m_boundSensorBuffer{VK_NULL_HANDLE};
  VkBuffer m_boundPointBuffer{VK_NULL_HANDLE};
};
