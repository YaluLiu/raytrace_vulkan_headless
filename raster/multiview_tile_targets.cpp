#include "multiview_tile_targets.hpp"

#include <array>
#include <stdexcept>

#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"

void MultiviewTileTargets::setup(VkDevice device,
                            uint32_t graphicsQueueIndex,
                            nvvk::ResourceAllocatorDma& allocator,
                            nvvk::DebugUtil& debug)
{
  _device             = device;
  _graphicsQueueIndex = graphicsQueueIndex;
  _allocator          = &allocator;
  _debug              = &debug;
}

void MultiviewTileTargets::destroy()
{
  destroyFramebuffer();
  destroyImages();
  _tileExtent = {0, 0};
  _layerCount = 0;
  _renderPass = VK_NULL_HANDLE;
}

bool MultiviewTileTargets::ensureResources(const TileAtlasConfig& config,
                                      const PreviewRasterPipeline& pipeline,
                                      VkRenderPass renderPass,
                                      uint32_t layerCount)
{
  if(_device == VK_NULL_HANDLE || _allocator == nullptr || renderPass == VK_NULL_HANDLE || layerCount == 0)
  {
    return false;
  }

  TileAtlasConfig sanitized = config;
  sanitized.sanitize();
  const VkExtent2D desiredExtent = sanitized.tileExtent();

  if(hasImages() && desiredExtent.width == _tileExtent.width && desiredExtent.height == _tileExtent.height
     && layerCount == _layerCount && renderPass == _renderPass && _colorFormat == pipeline.getColorFormat()
     && _depthFormat == pipeline.getDepthAovFormat()
     && _depthAttachmentFormat == pipeline.getDepthAttachmentFormat())
  {
    if(!hasFramebuffer())
    {
      createFramebuffer(renderPass);
    }
    return hasFramebuffer();
  }

  destroyFramebuffer();
  destroyImages();

  _tileExtent            = desiredExtent;
  _layerCount            = layerCount;
  _renderPass            = renderPass;
  _colorFormat           = pipeline.getColorFormat();
  _depthFormat           = pipeline.getDepthAovFormat();
  _depthAttachmentFormat = pipeline.getDepthAttachmentFormat();

  constexpr VkImageUsageFlags kAovUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                          | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                          | VK_IMAGE_USAGE_STORAGE_BIT;
  createLayerImage(_colorLayers, _colorFormat, kAovUsage, "TileMultiviewColorLayers");
  createLayerImage(_depthLayers, _depthFormat, kAovUsage, "TileMultiviewDepthLayers");
  createDepthAttachment("TileMultiviewDepthAttachment");

  nvvk::CommandPool commandPool(_device, _graphicsQueueIndex);
  VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
  nvvk::cmdBarrierImageLayout(cmdBuf, _colorLayers.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _depthLayers.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, _depthAttachment.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
  commandPool.submitAndWait(cmdBuf);

  createFramebuffer(renderPass);
  return hasImages() && hasFramebuffer();
}

bool MultiviewTileTargets::hasImages() const
{
  return _colorLayers.image != VK_NULL_HANDLE && _depthLayers.image != VK_NULL_HANDLE
         && _depthAttachment.image != VK_NULL_HANDLE && _tileExtent.width > 0 && _tileExtent.height > 0
         && _layerCount > 0;
}

void MultiviewTileTargets::createLayerImage(nvvk::Texture& texture,
                                       VkFormat format,
                                       VkImageUsageFlags usage,
                                       const char* debugName)
{
  auto imageInfo        = nvvk::makeImage2DCreateInfo(_tileExtent, format, usage);
  imageInfo.arrayLayers = _layerCount;
  nvvk::Image image     = _allocator->createImage(imageInfo);

  VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image                           = image.image;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  viewInfo.format                          = format;
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = _layerCount;

  texture                        = _allocator->createTexture(image, viewInfo);
  texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  if(_debug != nullptr && texture.image != VK_NULL_HANDLE)
  {
    _debug->setObjectName(texture.image, debugName);
  }
}

void MultiviewTileTargets::createDepthAttachment(const char* debugName)
{
  auto imageInfo        = nvvk::makeImage2DCreateInfo(_tileExtent, _depthAttachmentFormat,
                                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  imageInfo.arrayLayers = _layerCount;
  nvvk::Image image     = _allocator->createImage(imageInfo);

  VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image                           = image.image;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  viewInfo.format                          = _depthAttachmentFormat;
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = _layerCount;

  _depthAttachment = _allocator->createTexture(image, viewInfo);

  if(_debug != nullptr && _depthAttachment.image != VK_NULL_HANDLE)
  {
    _debug->setObjectName(_depthAttachment.image, debugName);
  }
}

void MultiviewTileTargets::createFramebuffer(VkRenderPass renderPass)
{
  if(!hasImages() || renderPass == VK_NULL_HANDLE)
  {
    return;
  }

  destroyFramebuffer();

  std::array<VkImageView, 3> attachments{_colorLayers.descriptor.imageView, _depthLayers.descriptor.imageView,
                                         _depthAttachment.descriptor.imageView};

  VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebufferInfo.renderPass      = renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments    = attachments.data();
  framebufferInfo.width           = _tileExtent.width;
  framebufferInfo.height          = _tileExtent.height;
  framebufferInfo.layers          = 1;

  if(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffer) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create multiview tile framebuffer");
  }
}

void MultiviewTileTargets::destroyFramebuffer()
{
  if(_device != VK_NULL_HANDLE && _framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(_device, _framebuffer, nullptr);
  }
  _framebuffer = VK_NULL_HANDLE;
}

void MultiviewTileTargets::destroyImages()
{
  if(_allocator != nullptr)
  {
    _allocator->destroy(_colorLayers);
    _allocator->destroy(_depthLayers);
    _allocator->destroy(_depthAttachment);
  }
  _colorLayers        = {};
  _depthLayers        = {};
  _depthAttachment    = {};
}
