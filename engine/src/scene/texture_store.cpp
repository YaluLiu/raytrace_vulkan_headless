#include "scene/texture_store.hpp"

#include <array>
#include <cfloat>
#include <cstdint>
#include <limits>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"

void TextureStore::setup(VkDevice device, uint32_t graphicsQueueIndex, nvvk::ResourceAllocatorDma& allocator)
{
  m_device = device;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_alloc = &allocator;
}

void TextureStore::destroy()
{
  if(m_alloc != nullptr)
  {
    for(auto& texture : m_textures)
    {
      m_alloc->destroy(texture);
    }
  }
  m_textures.clear();
}

void TextureStore::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
  uploadTextureResources(cmdBuf, textureAssets);
  cmdBufGet.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();
}

void TextureStore::rebuildTextureResources(const std::vector<TextureAsset>& textureAssets)
{
  destroy();
  loadTextureAssets(textureAssets);
}

void TextureStore::uploadTextureResources(const VkCommandBuffer& cmdBuf, const std::vector<TextureAsset>& textureAssets)
{
  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod = FLT_MAX;

  auto formatForColorSpace = [](TextureColorSpace colorSpace)
  { return colorSpace == TextureColorSpace::Linear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB; };

  auto uploadTexture = [&](const stbi_uc* pixels, int texWidth, int texHeight, TextureColorSpace colorSpace)
  {
    VkFormat format = formatForColorSpace(colorSpace);
    VkDeviceSize bufferSize = static_cast<uint64_t>(texWidth) * texHeight * sizeof(uint8_t) * 4;
    auto imgSize = VkExtent2D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight)};
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

    nvvk::Image image = m_alloc->createImage(cmdBuf, bufferSize, pixels, imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture texture = m_alloc->createTexture(image, ivInfo, samplerCreateInfo);

    m_textures.push_back(texture);
  };

  if(textureAssets.empty() && m_textures.empty())
  {
    nvvk::Texture texture;

    std::array<uint8_t, 4> color{255u, 255u, 255u, 255u};
    VkDeviceSize bufferSize = sizeof(color);
    auto imgSize = VkExtent2D{1, 1};
    VkFormat format = formatForColorSpace(TextureColorSpace::SRGB);
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format);

    nvvk::Image image = m_alloc->createImage(cmdBuf, bufferSize, color.data(), imageCreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    texture = m_alloc->createTexture(image, ivInfo, samplerCreateInfo);

    nvvk::cmdBarrierImageLayout(cmdBuf, texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_textures.push_back(texture);
  }
  else
  {
    for(const TextureAsset& textureAsset : textureAssets)
    {
      int texWidth = 0;
      int texHeight = 0;
      int texChannels = 0;
      stbi_uc* stbi_pixels = nullptr;
      if(!textureAsset.encodedBytes.empty() &&
         textureAsset.encodedBytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max()))
      {
        stbi_pixels =
            stbi_load_from_memory(textureAsset.encodedBytes.data(), static_cast<int>(textureAsset.encodedBytes.size()),
                                  &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
      }

      std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};

      stbi_uc* pixels = stbi_pixels;
      if(!stbi_pixels)
      {
        texWidth = texHeight = 1;
        texChannels = 4;
        pixels = reinterpret_cast<stbi_uc*>(color.data());
      }

      uploadTexture(pixels, texWidth, texHeight, textureAsset.colorSpace);

      stbi_image_free(stbi_pixels);
    }
  }
}
