#pragma once

#include <engine/renderer_types.hpp>

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"

#include <string>

#include <vulkan/vulkan_core.h>

struct SensorGenerationPipelineConfig
{
  const char* debugLabel{nullptr};
  const char* shaderSpvPath{nullptr};
  VkDeviceSize pushConstantSize{0};
};

class SensorGenerationPipeline final
{
public:
  void setup(VkDevice device, nvvk::DebugUtil& debug, SensorGenerationPipelineConfig config);
  void destroy();

  bool ensureResources(const TlasDescriptorInfo& tlasInfo, VkBuffer sensorBuffer, VkBuffer outputBuffer);
  void dispatch(const VkCommandBuffer& cmdBuf, const void* pushConstant, uint32_t width, uint32_t height);

private:
  void createDescriptorResources();
  void createPipeline();
  void updateDescriptorSet(const TlasDescriptorInfo& tlasInfo, VkBuffer sensorBuffer, VkBuffer outputBuffer);

  VkDevice m_device{VK_NULL_HANDLE};
  nvvk::DebugUtil* m_debug{nullptr};
  std::string m_debugLabel;
  std::string m_shaderSpvPath;
  VkDeviceSize m_pushConstantSize{0};
  nvvk::DescriptorSetBindings m_bindings;
  VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkAccelerationStructureKHR m_boundTlas{VK_NULL_HANDLE};
  VkBuffer m_boundSensorBuffer{VK_NULL_HANDLE};
  VkBuffer m_boundOutputBuffer{VK_NULL_HANDLE};
};
