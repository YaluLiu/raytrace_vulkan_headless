#pragma once

#include "aov_texture.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "tile_config.hpp"

#include <cstdint>
#include <optional>

class TileAovChannelAtlas final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  bool ensureResources(const TileAtlasConfig& config, VkFormat format, const char* debugName, bool exported);
  bool copyLayersFrom(const VkCommandBuffer& cmdBuf,
                      const nvvk::Texture& sourceTexture,
                      uint32_t firstTileIndex,
                      uint32_t tileCount,
                      uint32_t sourceLayerCount);

  bool hasImage() const;
  VkExtent2D getExtent() const { return m_atlasExtent; }
  const TileAtlasConfig& getConfig() const { return m_config; }
  VkRect2D getTileRect(uint32_t tileIndex) const;
  const nvvk::Texture& getTexture() const { return m_atlas; }
  std::optional<ExportedRasterAovTexture> getAovTexture() const;

private:
  void createAtlasImage(VkFormat format, VkExtent2D extent, const char* debugName, bool exported);
  void destroyImage();

  VkDevice m_device{VK_NULL_HANDLE};
  VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};
  mutable nvvk::ExportResourceAllocatorDedicated m_exportAllocator;

  TileAtlasConfig m_config;
  VkExtent2D m_atlasExtent{0, 0};
  nvvk::Texture m_atlas;
  VkFormat m_format{VK_FORMAT_UNDEFINED};
  bool m_exported{false};
};
