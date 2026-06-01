#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "features/preview/preview_pipeline.hpp"
#include <engine/tile_config.hpp>

class MultiviewTileTargets final
{
public:
  void setup(VkDevice device, uint32_t graphicsQueueIndex, nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug);
  void destroy();

  bool ensureResources(const TileAtlasConfig& config,
                       const PreviewPipeline& pipeline,
                       VkRenderPass renderPass,
                       uint32_t layerCount);

  bool hasImages() const;
  bool hasFramebuffer() const { return _framebuffer != VK_NULL_HANDLE; }
  VkExtent2D getExtent() const { return _tileExtent; }
  uint32_t getLayerCount() const { return _layerCount; }
  VkFramebuffer getFramebuffer() const { return _framebuffer; }
  const nvvk::Texture& getColorTexture() const { return _colorLayers; }
  const nvvk::Texture& getDepthAovTexture() const { return _depthLayers; }
  const nvvk::Texture& getDepthAttachmentTexture() const { return _depthAttachment; }

private:
  void createLayerImage(nvvk::Texture& texture, VkFormat format, VkImageUsageFlags usage, const char* debugName);
  void createDepthAttachment(const char* debugName);
  void createFramebuffer(VkRenderPass renderPass);
  void destroyFramebuffer();
  void destroyImages();

  VkDevice _device{VK_NULL_HANDLE};
  uint32_t _graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* _allocator{nullptr};
  nvvk::DebugUtil* _debug{nullptr};

  VkExtent2D _tileExtent{0, 0};
  uint32_t _layerCount{0};
  VkRenderPass _renderPass{VK_NULL_HANDLE};
  VkFramebuffer _framebuffer{VK_NULL_HANDLE};

  nvvk::Texture _colorLayers;
  VkFormat _colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture _depthLayers;
  VkFormat _depthFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture _depthAttachment;
  VkFormat _depthAttachmentFormat{VK_FORMAT_X8_D24_UNORM_PACK32};
};
