#pragma once

#include "aov_texture.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "raster_pipeline.hpp"
#include "tile_config.hpp"
#include "tile_layer_output.hpp"

class TileAtlasOutput final
{
public:
  void setup(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma &allocator, nvvk::DebugUtil &debug);
  void destroy();

  bool ensureResources(const HeadlessTileConfig &config, const RasterPipeline &pipeline);
  bool copyLayersFrom(const VkCommandBuffer &cmdBuf, const TileLayerOutput &layeredOutput, uint32_t firstTileIndex,
                      uint32_t tileCount);

  bool hasImages() const;
  bool hasFramebuffer() const
  {
    return _framebuffer != VK_NULL_HANDLE;
  }
  VkExtent2D getExtent() const
  {
    return _atlasExtent;
  }
  const HeadlessTileConfig &getConfig() const
  {
    return _config;
  }
  VkFramebuffer getFramebuffer() const
  {
    return _framebuffer;
  }
  VkRect2D getTileRect(uint32_t tileIndex) const;
  const nvvk::Texture &getColorTexture() const
  {
    return _colorAtlas;
  }
  const nvvk::Texture &getDepthTexture() const
  {
    return _depthAtlas;
  }
  const nvvk::Texture &getObjectIdTexture() const
  {
    return _objectIdAtlas;
  }
  const nvvk::Texture &getInstanceIdTexture() const
  {
    return _instanceIdAtlas;
  }
  std::optional<HeadlessAovTexture> getAovTexture(HeadlessAov aov) const;

private:
  void createExportedAtlasImage(nvvk::Texture &texture, VkFormat format, VkExtent2D extent, const char *debugName);
  void createAtlasImage(nvvk::Texture &texture, VkFormat format, VkExtent2D extent, const char *debugName);
  void createDepthAttachment(VkExtent2D extent, const char *debugName);
  void createFramebuffer(VkRenderPass renderPass);
  void destroyFramebuffer();
  void destroyImages();

  VkDevice _device{VK_NULL_HANDLE};
  VkPhysicalDevice _physicalDevice{VK_NULL_HANDLE};
  uint32_t _graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma *_allocator{nullptr};
  nvvk::DebugUtil *_debug{nullptr};
  mutable nvvk::ExportResourceAllocatorDedicated _exportAllocator;

  HeadlessTileConfig _config;
  VkExtent2D _atlasExtent{0, 0};
  VkRenderPass _renderPass{VK_NULL_HANDLE};
  VkFramebuffer _framebuffer{VK_NULL_HANDLE};

  nvvk::Texture _colorAtlas;
  VkFormat _colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture _objectIdAtlas;
  VkFormat _objectIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture _instanceIdAtlas;
  VkFormat _instanceIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture _depthAtlas;
  VkFormat _depthFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture _depthAttachment;
  VkFormat _depthAttachmentFormat{VK_FORMAT_X8_D24_UNORM_PACK32};
};
