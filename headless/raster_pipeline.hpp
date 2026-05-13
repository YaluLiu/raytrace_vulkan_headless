#pragma once

#include <optional>
#include <span>

#include <vulkan/vulkan_core.h>

#include "aov_texture.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "raster_scene_types.hpp"
#include "shaders/host_device.h"

class RasterPipeline final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& transientAllocator,
             nvvk::DebugUtil& debug);
  void destroy();

  void createOffscreenRender(VkExtent2D size);
  void createGraphicsPipeline(VkDescriptorSetLayout sceneDescriptorSetLayout);
  void destroyGraphicsPipeline();

  void rasterize(const VkCommandBuffer& cmdBuf,
                 VkDescriptorSet sceneDescriptorSet,
                 std::span<const RasterObjModel> objModels,
                 std::span<const RasterObjInstance> instances,
                 std::span<const int> instanceIds);

  std::optional<HeadlessAovTexture> getAovTexture(HeadlessAov aov) const;
  VkExtent2D getRenderSize() const { return _renderSize; }
  VkExtent2D getAovSize() const { return _aovSize; }

private:
  void createOffscreenImage(nvvk::Texture& texture, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent);
  void createFramebuffer();
  void destroyFramebuffer();

  VkDevice         _device{VK_NULL_HANDLE};
  VkPhysicalDevice _physicalDevice{VK_NULL_HANDLE};
  uint32_t         _graphicsQueueIndex{0};

  nvvk::ResourceAllocatorDma*            _transientAllocator{nullptr};
  nvvk::DebugUtil*                       _debug{nullptr};
  nvvk::ExportResourceAllocatorDedicated _sharedAlloc;

  nvvk::Texture _offscreenColor;
  VkFormat      _offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture _offscreenObjectId;
  VkFormat      _offscreenObjectIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture _offscreenInstanceId;
  VkFormat      _offscreenInstanceIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture _offscreenDepthAov;
  VkFormat      _offscreenDepthAovFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture _offscreenDepth;
  VkFormat      _offscreenDepthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  VkExtent2D _renderSize{0, 0};
  VkExtent2D _aovSize{0, 0};

  VkRenderPass     _renderPass{VK_NULL_HANDLE};
  VkFramebuffer    _framebuffer{VK_NULL_HANDLE};
  VkPipeline       _rasterPipeline{VK_NULL_HANDLE};
  VkPipeline       _domeBackgroundPipeline{VK_NULL_HANDLE};
  VkPipelineLayout _pipelineLayout{VK_NULL_HANDLE};
};
