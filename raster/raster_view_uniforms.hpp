#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "raster_renderer_types.hpp"
#include "shaders/host_device.h"

#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

class RasterViewUniforms final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  bool ensureFrameUniformCapacity(uint32_t slotCount);
  bool ensureTileFrameUniformBuffer();

  void updateFrameUniformBuffer(const VkCommandBuffer& cmdBuf, VkExtent2D renderSize, size_t lightCount);
  void updateTileFrameUniformBufferForBatch(const VkCommandBuffer& cmdBuf,
                                            uint32_t firstCameraIndex,
                                            uint32_t cameraCount,
                                            uint32_t viewCount,
                                            VkExtent2D tileExtent,
                                            size_t lightCount);

  void setCameras(std::vector<RasterCameraSpec> cameras);
  const std::vector<RasterCameraSpec>& getCameras() const { return m_cameras; }
  void setMainCamera(const RasterCameraSpec& camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  float getMainCameraClipStart() const { return m_mainCameraClipStart; }
  float getMainCameraClipEnd() const { return m_mainCameraClipEnd; }

  VkDescriptorBufferInfo getFrameUniformDescriptorInfo() const;
  VkDescriptorBufferInfo getTileFrameUniformDescriptorInfo() const;

private:
  VkDevice m_device{VK_NULL_HANDLE};
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  nvvk::Buffer m_bFrameUniforms;
  VkDeviceSize m_frameUniformStride{sizeof(FrameUniforms)};
  uint32_t m_frameUniformSlotCount{0};
  nvvk::Buffer m_bTileFrameUniforms;

  std::vector<RasterCameraSpec> m_cameras;
  float m_mainCameraClipStart{0.1f};
  float m_mainCameraClipEnd{1000.0f};
};
