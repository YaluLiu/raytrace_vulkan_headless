#include "output/tile/tile_aov_channel.hpp"

#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "raster_image_barriers.hpp"

#include <algorithm>

namespace
{
bool requiresDedicatedImageAllocation(VkDevice device, VkImage image)
{
  if(device == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
  {
    return false;
  }

  VkMemoryDedicatedRequirements dedicatedRequirements{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
  VkMemoryRequirements2 memoryRequirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  VkImageMemoryRequirementsInfo2 imageRequirements{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
  imageRequirements.image = image;
  memoryRequirements.pNext = &dedicatedRequirements;

  vkGetImageMemoryRequirements2(device, &imageRequirements, &memoryRequirements);
  return dedicatedRequirements.requiresDedicatedAllocation == VK_TRUE;
}
} // namespace

void TileAovChannelAtlas::setup(VkDevice device,
                                VkPhysicalDevice physicalDevice,
                                uint32_t graphicsQueueIndex,
                                nvvk::ResourceAllocatorDma& allocator,
                                nvvk::DebugUtil& debug)
{
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_allocator = &allocator;
  m_debug = &debug;
  m_exportAllocator.init(device, physicalDevice);
}

void TileAovChannelAtlas::destroy()
{
  destroyImage();
  if(m_device != VK_NULL_HANDLE)
  {
    m_exportAllocator.deinit();
  }
  m_device = VK_NULL_HANDLE;
  m_physicalDevice = VK_NULL_HANDLE;
  m_graphicsQueueIndex = 0;
  m_allocator = nullptr;
  m_debug = nullptr;
  m_config = {};
  m_atlasExtent = {0, 0};
  m_format = VK_FORMAT_UNDEFINED;
  m_exported = false;
}

bool TileAovChannelAtlas::ensureResources(const TileAtlasConfig& config,
                                          VkFormat format,
                                          const char* debugName,
                                          bool exported)
{
  if(m_device == VK_NULL_HANDLE || m_allocator == nullptr || format == VK_FORMAT_UNDEFINED)
  {
    return false;
  }

  TileAtlasConfig sanitized = config;
  sanitized.sanitize();
  const VkExtent2D desiredExtent = sanitized.atlasExtent();

  if(hasImage() && sanitized == m_config && desiredExtent.width == m_atlasExtent.width &&
     desiredExtent.height == m_atlasExtent.height && format == m_format && exported == m_exported)
  {
    return true;
  }

  destroyImage();

  m_config = sanitized;
  m_atlasExtent = desiredExtent;
  m_format = format;
  m_exported = exported;

  createAtlasImage(m_format, m_atlasExtent, debugName, m_exported);

  nvvk::CommandPool commandPool(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
  nvvk::cmdBarrierImageLayout(cmdBuf, m_atlas.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  commandPool.submitAndWait(cmdBuf);

  return hasImage();
}

bool TileAovChannelAtlas::copyLayersFrom(const VkCommandBuffer& cmdBuf,
                                         const nvvk::Texture& sourceTexture,
                                         uint32_t firstTileIndex,
                                         uint32_t tileCount,
                                         uint32_t sourceLayerCount)
{
  if(!hasImage() || sourceTexture.image == VK_NULL_HANDLE || tileCount == 0 || sourceLayerCount == 0 ||
     firstTileIndex >= m_config.capacity())
  {
    return false;
  }

  const uint32_t boundedTileCount = std::min({tileCount, sourceLayerCount, m_config.capacity() - firstTileIndex});
  if(boundedTileCount == 0)
  {
    return false;
  }

  const VkImageMemoryBarrier beforeBarriers[2] = {
      makeColorImageBarrier(sourceTexture.image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, sourceLayerCount),
      makeColorImageBarrier(m_atlas.image,
                            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
  };
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, beforeBarriers);

  for(uint32_t layerIndex = 0; layerIndex < boundedTileCount; ++layerIndex)
  {
    const VkRect2D tileRect = getTileRect(firstTileIndex + layerIndex);
    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layerIndex, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffset = {tileRect.offset.x, tileRect.offset.y, 0};
    region.extent = {tileRect.extent.width, tileRect.extent.height, 1};
    vkCmdCopyImage(cmdBuf, sourceTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_atlas.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

  const VkImageMemoryBarrier afterBarriers[2] = {
      makeColorImageBarrier(sourceTexture.image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, sourceLayerCount),
      makeColorImageBarrier(m_atlas.image, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
  };
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, afterBarriers);
  return true;
}

bool TileAovChannelAtlas::hasImage() const
{
  return m_atlas.image != VK_NULL_HANDLE && m_atlas.descriptor.imageView != VK_NULL_HANDLE && m_atlasExtent.width > 0 &&
         m_atlasExtent.height > 0;
}

VkRect2D TileAovChannelAtlas::getTileRect(uint32_t tileIndex) const
{
  if(m_config.capacity() == 0)
  {
    return {};
  }

  const uint32_t column = tileIndex % m_config.gridColumns;
  const uint32_t row = tileIndex / m_config.gridColumns;
  VkRect2D rect{};
  rect.offset = {static_cast<int32_t>(column * m_config.cameraWidth),
                 static_cast<int32_t>(row * m_config.cameraHeight)};
  rect.extent = {m_config.cameraWidth, m_config.cameraHeight};
  return rect;
}

std::optional<ExportedRasterAovTexture> TileAovChannelAtlas::getAovTexture() const
{
  if(!hasImage() || !m_exported || m_atlas.memHandle == nullptr)
  {
    return std::nullopt;
  }

  auto* memAlloc = m_exportAllocator.getMemoryAllocator();
  if(memAlloc == nullptr)
  {
    return std::nullopt;
  }

  const auto memoryInfo = memAlloc->getMemoryInfo(m_atlas.memHandle);
  if(memoryInfo.memory == VK_NULL_HANDLE || memoryInfo.size == 0)
  {
    return std::nullopt;
  }

  ExportedRasterAovTexture result;
  result.device = m_device;
  result.image = m_atlas.image;
  result.imageView = m_atlas.descriptor.imageView;
  result.memory = memoryInfo.memory;
  result.memoryOffset = memoryInfo.offset;
  result.memorySize = memoryInfo.size;
  result.format = m_format;
  result.extent = m_atlasExtent;
  result.layout = m_atlas.descriptor.imageLayout;
  result.dedicatedMemory = requiresDedicatedImageAllocation(m_device, m_atlas.image);
  return result;
}

void TileAovChannelAtlas::createAtlasImage(VkFormat format, VkExtent2D extent, const char* debugName, bool exported)
{
  constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  auto imageInfo = nvvk::makeImage2DCreateInfo(extent, format, usage);
  VkImageViewCreateInfo imageViewInfo{};
  if(exported)
  {
    nvvk::Image image = m_exportAllocator.createImage(imageInfo);
    imageViewInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_atlas = m_exportAllocator.createTexture(image, imageViewInfo, samplerInfo);
  }
  else
  {
    nvvk::Image image = m_allocator->createImage(imageInfo);
    imageViewInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
    m_atlas = m_allocator->createTexture(image, imageViewInfo);
  }
  m_atlas.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  if(m_debug != nullptr && m_atlas.image != VK_NULL_HANDLE)
  {
    m_debug->setObjectName(m_atlas.image, debugName);
  }
}

void TileAovChannelAtlas::destroyImage()
{
  if(m_exported)
  {
    if(m_device != VK_NULL_HANDLE)
    {
      m_exportAllocator.destroy(m_atlas);
    }
  }
  else if(m_allocator != nullptr)
  {
    m_allocator->destroy(m_atlas);
  }
  m_atlas = {};
}
