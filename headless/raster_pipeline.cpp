#include "raster_pipeline.hpp"

void RasterPipeline::setup(VkDevice device,
                           VkPhysicalDevice physicalDevice,
                           uint32_t graphicsQueueIndex,
                           nvvk::ResourceAllocatorDma& transientAllocator,
                           nvvk::DebugUtil& debug)
{
  _device             = device;
  _physicalDevice     = physicalDevice;
  _graphicsQueueIndex = graphicsQueueIndex;
  _transientAllocator = &transientAllocator;
  _debug              = &debug;
  _sharedAlloc.init(device, physicalDevice);
}

void RasterPipeline::destroy()
{
  destroyGraphicsPipeline();
  if(_transientAllocator != nullptr)
  {
    _transientAllocator->destroy(_offscreenDepth);
  }
  _sharedAlloc.destroy(_offscreenColor);
  _sharedAlloc.destroy(_offscreenObjectId);
  _sharedAlloc.destroy(_offscreenInstanceId);
  _sharedAlloc.destroy(_offscreenDepthAov);
  _sharedAlloc.deinit();
}

void RasterPipeline::destroyGraphicsPipeline()
{
  destroyFramebuffer();
  if(_device != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(_device, _rasterPipeline, nullptr);
    vkDestroyPipeline(_device, _domeBackgroundPipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
    vkDestroyRenderPass(_device, _renderPass, nullptr);
  }
  _rasterPipeline         = VK_NULL_HANDLE;
  _domeBackgroundPipeline = VK_NULL_HANDLE;
  _pipelineLayout         = VK_NULL_HANDLE;
  _renderPass             = VK_NULL_HANDLE;
}

void RasterPipeline::createOffscreenRender(VkExtent2D) {}

void RasterPipeline::createGraphicsPipeline(VkDescriptorSetLayout) {}

void RasterPipeline::rasterize(const VkCommandBuffer&,
                               VkDescriptorSet,
                               std::span<const RasterObjModel>,
                               std::span<const RasterObjInstance>,
                               std::span<const int>)
{
}

std::optional<HeadlessAovTexture> RasterPipeline::getAovTexture(HeadlessAov) const
{
  return std::nullopt;
}

void RasterPipeline::createOffscreenImage(nvvk::Texture&, VkFormat, VkImageUsageFlags, VkExtent2D) {}

void RasterPipeline::createFramebuffer() {}

void RasterPipeline::destroyFramebuffer() {}
