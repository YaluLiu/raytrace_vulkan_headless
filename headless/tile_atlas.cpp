#include "tile_atlas.hpp"

#include <array>

#include "hello_vulkan_barriers.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"

void TileAtlasOutput::setup(VkDevice device, uint32_t graphicsQueueIndex,
                            nvvk::ResourceAllocatorDma &allocator,
                            nvvk::DebugUtil &debug) {
  _device = device;
  _graphicsQueueIndex = graphicsQueueIndex;
  _allocator = &allocator;
  _debug = &debug;
}

void TileAtlasOutput::destroy() {
  if (_allocator != nullptr) {
    _allocator->destroy(_colorAtlas);
    _allocator->destroy(_depthAtlas);
  }
  _colorAtlas = {};
  _depthAtlas = {};
  _atlasExtent = {0, 0};
  _config = {};
}

bool TileAtlasOutput::ensureResources(const HeadlessTileConfig &config) {
  if (_device == VK_NULL_HANDLE || _allocator == nullptr) {
    return false;
  }

  HeadlessTileConfig sanitized = config;
  sanitized.sanitize();
  const VkExtent2D desiredExtent = sanitized.atlasExtent();
  if (hasImages() && sanitized == _config &&
      desiredExtent.width == _atlasExtent.width &&
      desiredExtent.height == _atlasExtent.height) {
    return true;
  }

  _allocator->destroy(_colorAtlas);
  _allocator->destroy(_depthAtlas);
  _colorAtlas = {};
  _depthAtlas = {};

  _config = sanitized;
  _atlasExtent = desiredExtent;

  createAtlasImage(_colorAtlas, _colorFormat, _atlasExtent, "TileColorAtlas");
  createAtlasImage(_depthAtlas, _depthFormat, _atlasExtent, "TileDepthAtlas");

  nvvk::CommandPool commandPool(_device, _graphicsQueueIndex);
  VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
  nvvk::cmdBarrierImageLayout(cmdBuf, _colorAtlas.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _depthAtlas.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
  commandPool.submitAndWait(cmdBuf);

  return hasImages();
}

void TileAtlasOutput::copyTileFrom(const VkCommandBuffer &cmdBuf,
                                   const RasterPipeline &sourcePipeline,
                                   uint32_t tileIndex) {
  if (!hasImages() || tileIndex >= _config.capacity()) {
    return;
  }

  const nvvk::Texture &sourceColor =
      sourcePipeline.getColorTextureForReadback();
  const nvvk::Texture &sourceDepth =
      sourcePipeline.getDepthAovTextureForReadback();
  if (sourceColor.image == VK_NULL_HANDLE ||
      sourceDepth.image == VK_NULL_HANDLE) {
    return;
  }

  const uint32_t column = tileIndex % _config.gridColumns;
  const uint32_t row = tileIndex / _config.gridColumns;

  std::array<VkImageMemoryBarrier, 4> toTransfer{
      makeColorImageBarrier(
          sourceColor.image,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
              VK_ACCESS_SHADER_READ_BIT,
          VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      makeColorImageBarrier(
          sourceDepth.image,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
              VK_ACCESS_SHADER_READ_BIT,
          VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
      makeColorImageBarrier(
          _colorAtlas.image,
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
      makeColorImageBarrier(
          _depthAtlas.image,
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
  };
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, static_cast<uint32_t>(toTransfer.size()),
                       toTransfer.data());

  VkImageCopy copyRegion{};
  copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.dstOffset = {static_cast<int32_t>(column * _config.cameraWidth),
                          static_cast<int32_t>(row * _config.cameraHeight), 0};
  copyRegion.extent = {_config.cameraWidth, _config.cameraHeight, 1};

  vkCmdCopyImage(cmdBuf, sourceColor.image,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _colorAtlas.image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
  vkCmdCopyImage(cmdBuf, sourceDepth.image,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _depthAtlas.image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  std::array<VkImageMemoryBarrier, 4> toGeneral{
      makeColorImageBarrier(
          sourceColor.image, VK_ACCESS_TRANSFER_READ_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
              VK_ACCESS_SHADER_READ_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
      makeColorImageBarrier(
          sourceDepth.image, VK_ACCESS_TRANSFER_READ_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
              VK_ACCESS_SHADER_READ_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
      makeColorImageBarrier(
          _colorAtlas.image, VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
      makeColorImageBarrier(
          _depthAtlas.image, VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
  };
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, static_cast<uint32_t>(toGeneral.size()),
                       toGeneral.data());
}

bool TileAtlasOutput::hasImages() const {
  return _colorAtlas.image != VK_NULL_HANDLE &&
         _depthAtlas.image != VK_NULL_HANDLE && _atlasExtent.width > 0 &&
         _atlasExtent.height > 0;
}

void TileAtlasOutput::createAtlasImage(nvvk::Texture &texture, VkFormat format,
                                       VkExtent2D extent,
                                       const char *debugName) {
  constexpr VkImageUsageFlags usage =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  auto imageInfo = nvvk::makeImage2DCreateInfo(extent, format, usage);
  nvvk::Image image = _allocator->createImage(imageInfo);
  VkImageViewCreateInfo imageViewInfo =
      nvvk::makeImageViewCreateInfo(image.image, imageInfo);
  texture = _allocator->createTexture(image, imageViewInfo);
  texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  if (_debug != nullptr && texture.image != VK_NULL_HANDLE) {
    _debug->setObjectName(texture.image, debugName);
  }
}
