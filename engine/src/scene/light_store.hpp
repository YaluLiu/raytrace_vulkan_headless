#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "shaders/common/host_device.h"

#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

class LightStore final
{
public:
  void setup(nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug);
  void destroy();

  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);

  size_t size() const { return m_lights.size(); }
  std::span<const Light> lights() const { return m_lights; }
  const nvvk::Buffer& buffer() const { return m_buffer; }

private:
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};
  std::vector<Light> m_lights;
  nvvk::Buffer m_buffer;
};
