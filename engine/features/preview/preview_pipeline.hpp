#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <vulkan/vulkan_core.h>

#include <engine/aov_texture.hpp>
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "scene/scene_types.hpp"
#include "shaders/common/host_device.h"

class PreviewPipeline final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& transientAllocator,
             nvvk::DebugUtil& debug);
  void destroy();

  void recreateAovTargets(VkExtent2D size);
  void createGraphicsPipeline(VkDescriptorSetLayout sceneDescriptorSetLayout);
  void destroyGraphicsPipeline();

  void recordPreviewAovs(const VkCommandBuffer& cmdBuf,
                 VkDescriptorSet sceneDescriptorSet,
                 std::span<const MeshBuffers> objModels,
                 std::span<const SceneInstance> instances,
                 std::span<const int> instanceIds,
                 uint32_t frameUniformOffset = 0);

  std::optional<ExportedAovTexture> getAovTexture(Aov aov) const;
  VkExtent2D getRenderSize() const { return _renderSize; }
  VkExtent2D getAovSize() const { return _aovSize; }
  VkRenderPass getRenderPass() const { return _renderPass; }
  VkFormat getColorFormat() const { return _offscreenColorFormat; }
  VkFormat getObjectIdFormat() const { return _offscreenObjectIdFormat; }
  VkFormat getInstanceIdFormat() const { return _offscreenInstanceIdFormat; }
  VkFormat getDepthAovFormat() const { return _offscreenDepthAovFormat; }
  VkFormat getDepthAttachmentFormat() const { return _offscreenDepthFormat; }
  VkImageView getColorImageView() const { return _offscreenColor.descriptor.imageView; }
  VkImageView getDepthAttachmentImageView() const { return _offscreenDepth.descriptor.imageView; }
  const nvvk::Texture& getColorTextureForReadback() const { return _offscreenColor; }
  const nvvk::Texture& getDepthAovTextureForReadback() const { return _offscreenDepthAov; }
  const nvvk::Texture& getObjectIdTextureForReadback() const { return _offscreenObjectId; }

private:
  void drawWithTarget(const VkCommandBuffer& cmdBuf,
                           VkFramebuffer framebuffer,
                           VkExtent2D framebufferExtent,
                           VkRect2D renderArea,
                           VkViewport viewport,
                           VkRect2D scissor,
                           VkDescriptorSet sceneDescriptorSet,
                           std::span<const MeshBuffers> objModels,
                           std::span<const SceneInstance> instances,
                           std::span<const int> instanceIds,
                           uint32_t frameUniformOffset);
  void createOffscreenImage(nvvk::Texture& texture, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent);
  void createFramebuffer();
  void destroyFramebuffer();

  VkDevice         _device{VK_NULL_HANDLE};
  VkPhysicalDevice _physicalDevice{VK_NULL_HANDLE};
  uint32_t         _graphicsQueueIndex{0};

  nvvk::ResourceAllocatorDma*                    _transientAllocator{nullptr};
  nvvk::DebugUtil*                               _debug{nullptr};
  mutable nvvk::ExportResourceAllocatorDedicated _sharedAlloc;

  nvvk::Texture _offscreenColor;
  VkFormat      _offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture _offscreenObjectId;
  VkFormat      _offscreenObjectIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture _offscreenInstanceId;
  VkFormat      _offscreenInstanceIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture _offscreenDepthAov;
  VkFormat      _offscreenDepthAovFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture _offscreenDepth;
  VkFormat      _offscreenDepthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  VkExtent2D _renderSize{0, 0};
  VkExtent2D _aovSize{0, 0};

  VkRenderPass     _renderPass{VK_NULL_HANDLE};
  VkFramebuffer    _framebuffer{VK_NULL_HANDLE};
  VkPipeline       _graphicsPipeline{VK_NULL_HANDLE};
  VkPipeline       _domeBackgroundPipeline{VK_NULL_HANDLE};
  VkPipelineLayout _pipelineLayout{VK_NULL_HANDLE};
};
