#pragma once

#include <engine/texture_asset.hpp>

#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

class TextureStore final
{
public:
  void setup(VkDevice device, uint32_t graphicsQueueIndex, nvvk::ResourceAllocatorDma& allocator);
  void destroy();

  void loadTextureAssets(const std::vector<TextureAsset>& textureAssets);
  void rebuildTextureResources(const std::vector<TextureAsset>& textureAssets);
  void uploadTextureResources(const VkCommandBuffer& cmdBuf, const std::vector<TextureAsset>& textureAssets);

  std::span<const nvvk::Texture> textures() const { return m_textures; }
  uint32_t descriptorCount() const { return static_cast<uint32_t>(m_textures.size()); }

private:
  VkDevice m_device{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  std::vector<nvvk::Texture> m_textures;
};
