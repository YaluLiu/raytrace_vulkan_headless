#include "tile_atlas.hpp"

#include <algorithm>

#include "hello_vulkan_barriers.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"

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

bool copyLayeredImageToAtlas(const VkCommandBuffer &cmdBuf, const nvvk::Texture &source,
                             const nvvk::Texture &destination, const TileAtlasOutput &atlas, uint32_t firstTileIndex,
                             uint32_t tileCount, uint32_t sourceLayerCount)
{
  if(source.image == VK_NULL_HANDLE || destination.image == VK_NULL_HANDLE || tileCount == 0 || sourceLayerCount == 0)
  {
    return false;
  }

  const VkImageMemoryBarrier beforeBarriers[2] = {
      makeColorImageBarrier(source.image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, sourceLayerCount),
      makeColorImageBarrier(destination.image,
                            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
  };
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, beforeBarriers);

  for(uint32_t layerIndex = 0; layerIndex < tileCount; ++layerIndex)
  {
    const VkRect2D tileRect = atlas.getTileRect(firstTileIndex + layerIndex);
    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layerIndex, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffset = {tileRect.offset.x, tileRect.offset.y, 0};
    region.extent = {tileRect.extent.width, tileRect.extent.height, 1};
    vkCmdCopyImage(cmdBuf, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

  const VkImageMemoryBarrier afterBarriers[2] = {
      makeColorImageBarrier(source.image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, sourceLayerCount),
      makeColorImageBarrier(destination.image, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
  };
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, afterBarriers);
  return true;
}
} // namespace

void TileAtlasOutput::setup(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsQueueIndex,
                            nvvk::ResourceAllocatorDma &allocator, nvvk::DebugUtil &debug)
{
  _device = device;
  _physicalDevice = physicalDevice;
  _graphicsQueueIndex = graphicsQueueIndex;
  _allocator = &allocator;
  _debug = &debug;
  _exportAllocator.init(device, physicalDevice);
}

void TileAtlasOutput::destroy()
{
  destroyImages();
  if(_device != VK_NULL_HANDLE)
  {
    _exportAllocator.deinit();
  }
  _device = VK_NULL_HANDLE;
  _physicalDevice = VK_NULL_HANDLE;
  _graphicsQueueIndex = 0;
  _allocator = nullptr;
  _debug = nullptr;
  _atlasExtent = {0, 0};
  _config = {};
}

bool TileAtlasOutput::ensureResources(const HeadlessTileConfig &config, const RasterPipeline &pipeline)
{
  if(_device == VK_NULL_HANDLE || _allocator == nullptr)
  {
    return false;
  }

  HeadlessTileConfig sanitized = config;
  sanitized.sanitize();
  const VkExtent2D desiredExtent = sanitized.atlasExtent();
  const VkFormat desiredColorFormat = pipeline.getColorFormat();
  const VkFormat desiredObjectIdFormat = pipeline.getObjectIdFormat();
  const VkFormat desiredInstanceIdFormat = pipeline.getInstanceIdFormat();
  const VkFormat desiredDepthFormat = pipeline.getDepthAovFormat();

  if(hasImages() && sanitized == _config && desiredExtent.width == _atlasExtent.width &&
     desiredExtent.height == _atlasExtent.height && _colorFormat == desiredColorFormat &&
     _objectIdFormat == desiredObjectIdFormat && _instanceIdFormat == desiredInstanceIdFormat &&
     _depthFormat == desiredDepthFormat)
  {
    return true;
  }

  destroyImages();

  _config = sanitized;
  _atlasExtent = desiredExtent;
  _colorFormat = desiredColorFormat;
  _objectIdFormat = desiredObjectIdFormat;
  _instanceIdFormat = desiredInstanceIdFormat;
  _depthFormat = desiredDepthFormat;

  createExportedAtlasImage(_colorAtlas, _colorFormat, _atlasExtent, "TileColorAtlas");
  createAtlasImage(_objectIdAtlas, _objectIdFormat, _atlasExtent, "TileObjectIdAtlas");
  createAtlasImage(_instanceIdAtlas, _instanceIdFormat, _atlasExtent, "TileInstanceIdAtlas");
  createExportedAtlasImage(_depthAtlas, _depthFormat, _atlasExtent, "TileDepthAtlas");

  nvvk::CommandPool commandPool(_device, _graphicsQueueIndex);
  VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
  nvvk::cmdBarrierImageLayout(cmdBuf, _colorAtlas.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _depthAtlas.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _objectIdAtlas.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _instanceIdAtlas.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  commandPool.submitAndWait(cmdBuf);

  return hasImages();
}

bool TileAtlasOutput::hasImages() const
{
  return _colorAtlas.image != VK_NULL_HANDLE && _objectIdAtlas.image != VK_NULL_HANDLE &&
         _instanceIdAtlas.image != VK_NULL_HANDLE && _depthAtlas.image != VK_NULL_HANDLE &&
         _atlasExtent.width > 0 && _atlasExtent.height > 0;
}

bool TileAtlasOutput::copyLayersFrom(const VkCommandBuffer &cmdBuf, const TileLayerOutput &layeredOutput,
                                     uint32_t firstTileIndex, uint32_t tileCount)
{
  if(!hasImages() || !layeredOutput.hasImages() || firstTileIndex >= _config.capacity())
  {
    return false;
  }

  const uint32_t boundedTileCount =
      std::min({tileCount, layeredOutput.getLayerCount(), _config.capacity() - firstTileIndex});
  if(boundedTileCount == 0)
  {
    return false;
  }

  const bool copiedColor = copyLayeredImageToAtlas(cmdBuf, layeredOutput.getColorTexture(), _colorAtlas, *this,
                                                   firstTileIndex, boundedTileCount, layeredOutput.getLayerCount());
  const bool copiedDepth = copyLayeredImageToAtlas(cmdBuf, layeredOutput.getDepthTexture(), _depthAtlas, *this,
                                                   firstTileIndex, boundedTileCount, layeredOutput.getLayerCount());
  const bool copiedObjectId = copyLayeredImageToAtlas(cmdBuf, layeredOutput.getObjectIdTexture(), _objectIdAtlas, *this,
                                                      firstTileIndex, boundedTileCount, layeredOutput.getLayerCount());
  const bool copiedInstanceId =
      copyLayeredImageToAtlas(cmdBuf, layeredOutput.getInstanceIdTexture(), _instanceIdAtlas, *this, firstTileIndex,
                              boundedTileCount, layeredOutput.getLayerCount());
  return copiedColor && copiedDepth && copiedObjectId && copiedInstanceId;
}

VkRect2D TileAtlasOutput::getTileRect(uint32_t tileIndex) const
{
  if(_config.capacity() == 0)
  {
    return {};
  }

  const uint32_t column = tileIndex % _config.gridColumns;
  const uint32_t row = tileIndex / _config.gridColumns;
  VkRect2D rect{};
  rect.offset = {static_cast<int32_t>(column * _config.cameraWidth), static_cast<int32_t>(row * _config.cameraHeight)};
  rect.extent = {_config.cameraWidth, _config.cameraHeight};
  return rect;
}

std::optional<HeadlessAovTexture> TileAtlasOutput::getAovTexture(HeadlessAov aov) const
{
  const nvvk::Texture *texture = nullptr;
  VkFormat format = VK_FORMAT_UNDEFINED;

  switch(aov)
  {
  case HeadlessAov::TileColor:
  case HeadlessAov::TileDisplayColor:
    texture = &_colorAtlas;
    format = _colorFormat;
    break;
  case HeadlessAov::TileDepth:
  case HeadlessAov::TileDisplayDepth:
    texture = &_depthAtlas;
    format = _depthFormat;
    break;
  case HeadlessAov::Color:
  case HeadlessAov::PrimId:
  case HeadlessAov::InstanceId:
  case HeadlessAov::Depth:
    return std::nullopt;
  }

  if(texture == nullptr || texture->image == VK_NULL_HANDLE || texture->descriptor.imageView == VK_NULL_HANDLE ||
     texture->memHandle == nullptr || _atlasExtent.width == 0 || _atlasExtent.height == 0)
  {
    return std::nullopt;
  }

  auto *memAlloc = _exportAllocator.getMemoryAllocator();
  if(memAlloc == nullptr)
  {
    return std::nullopt;
  }

  const auto memoryInfo = memAlloc->getMemoryInfo(texture->memHandle);
  if(memoryInfo.memory == VK_NULL_HANDLE || memoryInfo.size == 0)
  {
    return std::nullopt;
  }

  HeadlessAovTexture result;
  result.device = _device;
  result.image = texture->image;
  result.imageView = texture->descriptor.imageView;
  result.memory = memoryInfo.memory;
  result.memoryOffset = memoryInfo.offset;
  result.memorySize = memoryInfo.size;
  result.format = format;
  result.extent = _atlasExtent;
  result.layout = texture->descriptor.imageLayout;
  result.dedicatedMemory = requiresDedicatedImageAllocation(_device, texture->image);
  return result;
}

void TileAtlasOutput::createExportedAtlasImage(nvvk::Texture &texture, VkFormat format, VkExtent2D extent,
                                               const char *debugName)
{
  constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  auto imageInfo = nvvk::makeImage2DCreateInfo(extent, format, usage);
  nvvk::Image image = _exportAllocator.createImage(imageInfo);
  VkImageViewCreateInfo imageViewInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
  VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  texture = _exportAllocator.createTexture(image, imageViewInfo, samplerInfo);
  texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  if(_debug != nullptr && texture.image != VK_NULL_HANDLE)
  {
    _debug->setObjectName(texture.image, debugName);
  }
}

void TileAtlasOutput::createAtlasImage(nvvk::Texture &texture, VkFormat format, VkExtent2D extent,
                                       const char *debugName)
{
  constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  auto imageInfo = nvvk::makeImage2DCreateInfo(extent, format, usage);
  nvvk::Image image = _allocator->createImage(imageInfo);
  VkImageViewCreateInfo imageViewInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
  texture = _allocator->createTexture(image, imageViewInfo);
  texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  if(_debug != nullptr && texture.image != VK_NULL_HANDLE)
  {
    _debug->setObjectName(texture.image, debugName);
  }
}

void TileAtlasOutput::destroyImages()
{
  if(_allocator != nullptr)
  {
    _allocator->destroy(_objectIdAtlas);
    _allocator->destroy(_instanceIdAtlas);
  }
  if(_device != VK_NULL_HANDLE)
  {
    _exportAllocator.destroy(_colorAtlas);
    _exportAllocator.destroy(_depthAtlas);
  }
  _colorAtlas = {};
  _objectIdAtlas = {};
  _instanceIdAtlas = {};
  _depthAtlas = {};
}
