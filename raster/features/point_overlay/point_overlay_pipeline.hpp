#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"

#include <vulkan/vulkan_core.h>

#include <string>

class PreviewRasterPipeline;

struct PointOverlayPipelineConfig
{
  std::string debugLabel;
  std::string vertexShaderSpvPath;
  std::string fragmentShaderSpvPath;
  std::string errorPrefix;
};

class PointOverlayPipeline final
{
public:
  void setup(VkDevice device, nvvk::DebugUtil& debug, PointOverlayPipelineConfig config);
  void destroy();
  void destroyGraphicsPipeline();

  bool ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                       const PreviewRasterPipeline& previewPipeline,
                       VkBuffer sensorBuffer,
                       VkBuffer pointBuffer);
  void draw(const VkCommandBuffer& cmdBuf,
            const PreviewRasterPipeline& previewPipeline,
            VkDescriptorSet sceneDescriptorSet,
            uint32_t sensorIndex,
            float pointSizePixels,
            uint32_t vertexCount);

private:
  void createDescriptorResources();
  void updateDescriptorSet(VkBuffer sensorBuffer, VkBuffer pointBuffer);
  void createRenderPass();
  void createFramebuffer(const PreviewRasterPipeline& previewPipeline);
  void createGraphicsPipeline();
  void destroyFramebuffer();

  VkDevice m_device{VK_NULL_HANDLE};
  nvvk::DebugUtil* m_debug{nullptr};
  PointOverlayPipelineConfig m_config;
  VkDescriptorSetLayout m_sceneDescriptorSetLayout{VK_NULL_HANDLE};
  nvvk::DescriptorSetBindings m_bindings;
  VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkRenderPass m_renderPass{VK_NULL_HANDLE};
  VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
  VkFormat m_colorFormat{VK_FORMAT_UNDEFINED};
  VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
  VkExtent2D m_extent{0, 0};
  VkBuffer m_boundSensorBuffer{VK_NULL_HANDLE};
  VkBuffer m_boundPointBuffer{VK_NULL_HANDLE};
};
