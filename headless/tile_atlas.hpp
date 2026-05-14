#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "raster_pipeline.hpp"
#include "tile_config.hpp"

class TileAtlasOutput final {
public:
  void setup(VkDevice device, uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma &allocator, nvvk::DebugUtil &debug);
  void destroy();

  bool ensureResources(const HeadlessTileConfig &config);
  void copyTileFrom(const VkCommandBuffer &cmdBuf,
                    const RasterPipeline &sourcePipeline, uint32_t tileIndex);

  bool hasImages() const;
  VkExtent2D getExtent() const { return _atlasExtent; }
  const HeadlessTileConfig &getConfig() const { return _config; }
  const nvvk::Texture &getColorTexture() const { return _colorAtlas; }
  const nvvk::Texture &getDepthTexture() const { return _depthAtlas; }

private:
  void createAtlasImage(nvvk::Texture &texture, VkFormat format,
                        VkExtent2D extent, const char *debugName);

  VkDevice _device{VK_NULL_HANDLE};
  uint32_t _graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma *_allocator{nullptr};
  nvvk::DebugUtil *_debug{nullptr};

  HeadlessTileConfig _config;
  VkExtent2D _atlasExtent{0, 0};

  nvvk::Texture _colorAtlas;
  VkFormat _colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture _depthAtlas;
  VkFormat _depthFormat{VK_FORMAT_R32_SFLOAT};
};
