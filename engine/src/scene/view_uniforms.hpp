#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include <engine/renderer_types.hpp>
#include "shaders/common/host_device.h"

#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

struct MainViewState
{
  glm::mat4 view{1.0f};
  float verticalFovDegrees{45.0f};
  float clipStart{0.1f};
  float clipEnd{1000.0f};
};

class ViewUniforms final
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

  void setCameras(std::vector<CameraSpec> cameras);
  const std::vector<CameraSpec>& getCameras() const { return m_cameras; }
  void setMainViewState(MainViewState state);
  void setMainCamera(const CameraSpec& camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  float getMainCameraClipStart() const { return m_mainViewState.clipStart; }
  float getMainCameraClipEnd() const { return m_mainViewState.clipEnd; }

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

  std::vector<CameraSpec> m_cameras;
  MainViewState m_mainViewState;
};
