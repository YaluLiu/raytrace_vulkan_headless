#include "tile_atlas.hpp"

#include <array>
#include <stdexcept>

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
  destroyFramebuffer();
  destroyImages();
  _atlasExtent = {0, 0};
  _renderPass = VK_NULL_HANDLE;
  _config = {};
}

bool TileAtlasOutput::ensureResources(const HeadlessTileConfig &config,
                                      const RasterPipeline &pipeline) {
  if (_device == VK_NULL_HANDLE || _allocator == nullptr) {
    return false;
  }

  HeadlessTileConfig sanitized = config;
  sanitized.sanitize();
  const VkExtent2D desiredExtent = sanitized.atlasExtent();
  const VkRenderPass desiredRenderPass = pipeline.getRenderPass();
  if (desiredRenderPass == VK_NULL_HANDLE) {
    return false;
  }

  if (hasImages() && sanitized == _config &&
      desiredExtent.width == _atlasExtent.width &&
      desiredExtent.height == _atlasExtent.height &&
      _renderPass == desiredRenderPass) {
    if (!hasFramebuffer()) {
      createFramebuffer(desiredRenderPass);
    }
    return hasFramebuffer();
  }

  destroyFramebuffer();
  destroyImages();

  _config = sanitized;
  _atlasExtent = desiredExtent;
  _renderPass = desiredRenderPass;
  _colorFormat = pipeline.getColorFormat();
  _objectIdFormat = pipeline.getObjectIdFormat();
  _instanceIdFormat = pipeline.getInstanceIdFormat();
  _depthFormat = pipeline.getDepthAovFormat();
  _depthAttachmentFormat = pipeline.getDepthAttachmentFormat();

  createAtlasImage(_colorAtlas, _colorFormat, _atlasExtent, "TileColorAtlas");
  createAtlasImage(_objectIdAtlas, _objectIdFormat, _atlasExtent,
                   "TileObjectIdAtlas");
  createAtlasImage(_instanceIdAtlas, _instanceIdFormat, _atlasExtent,
                   "TileInstanceIdAtlas");
  createAtlasImage(_depthAtlas, _depthFormat, _atlasExtent, "TileDepthAtlas");
  createDepthAttachment(_atlasExtent, "TileDepthAttachment");

  nvvk::CommandPool commandPool(_device, _graphicsQueueIndex);
  VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
  nvvk::cmdBarrierImageLayout(cmdBuf, _colorAtlas.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _depthAtlas.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _objectIdAtlas.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _instanceIdAtlas.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _depthAttachment.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_ASPECT_DEPTH_BIT);
  commandPool.submitAndWait(cmdBuf);

  createFramebuffer(desiredRenderPass);

  return hasImages() && hasFramebuffer();
}

bool TileAtlasOutput::hasImages() const {
  return _colorAtlas.image != VK_NULL_HANDLE &&
         _objectIdAtlas.image != VK_NULL_HANDLE &&
         _instanceIdAtlas.image != VK_NULL_HANDLE &&
         _depthAtlas.image != VK_NULL_HANDLE &&
         _depthAttachment.image != VK_NULL_HANDLE &&
         _atlasExtent.width > 0 && _atlasExtent.height > 0;
}

VkRect2D TileAtlasOutput::getTileRect(uint32_t tileIndex) const {
  if (_config.capacity() == 0) {
    return {};
  }

  const uint32_t column = tileIndex % _config.gridColumns;
  const uint32_t row = tileIndex / _config.gridColumns;
  VkRect2D rect{};
  rect.offset = {static_cast<int32_t>(column * _config.cameraWidth),
                 static_cast<int32_t>(row * _config.cameraHeight)};
  rect.extent = {_config.cameraWidth, _config.cameraHeight};
  return rect;
}

void TileAtlasOutput::createAtlasImage(nvvk::Texture &texture, VkFormat format,
                                       VkExtent2D extent,
                                       const char *debugName) {
  constexpr VkImageUsageFlags usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_STORAGE_BIT;

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

void TileAtlasOutput::createDepthAttachment(VkExtent2D extent,
                                            const char *debugName) {
  auto imageInfo = nvvk::makeImage2DCreateInfo(
      extent, _depthAttachmentFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  nvvk::Image image = _allocator->createImage(imageInfo);

  VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = _depthAttachmentFormat;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  viewInfo.image = image.image;

  _depthAttachment = _allocator->createTexture(image, viewInfo);

  if (_debug != nullptr && _depthAttachment.image != VK_NULL_HANDLE) {
    _debug->setObjectName(_depthAttachment.image, debugName);
  }
}

void TileAtlasOutput::createFramebuffer(VkRenderPass renderPass) {
  if (!hasImages() || renderPass == VK_NULL_HANDLE) {
    return;
  }

  destroyFramebuffer();

  std::array<VkImageView, 5> attachments{
      _colorAtlas.descriptor.imageView, _objectIdAtlas.descriptor.imageView,
      _instanceIdAtlas.descriptor.imageView, _depthAtlas.descriptor.imageView,
      _depthAttachment.descriptor.imageView};

  VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebufferInfo.renderPass = renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = _atlasExtent.width;
  framebufferInfo.height = _atlasExtent.height;
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create tile atlas framebuffer");
  }
}

void TileAtlasOutput::destroyFramebuffer() {
  if (_device != VK_NULL_HANDLE && _framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(_device, _framebuffer, nullptr);
  }
  _framebuffer = VK_NULL_HANDLE;
}

void TileAtlasOutput::destroyImages() {
  if (_allocator != nullptr) {
    _allocator->destroy(_colorAtlas);
    _allocator->destroy(_objectIdAtlas);
    _allocator->destroy(_instanceIdAtlas);
    _allocator->destroy(_depthAtlas);
    _allocator->destroy(_depthAttachment);
  }
  _colorAtlas = {};
  _objectIdAtlas = {};
  _instanceIdAtlas = {};
  _depthAtlas = {};
  _depthAttachment = {};
}
