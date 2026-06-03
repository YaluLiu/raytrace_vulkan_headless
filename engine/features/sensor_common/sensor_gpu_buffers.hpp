#pragma once

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <limits>

#include <vulkan/vulkan_core.h>

class SensorGpuBuffers final
{
public:
  void setup(nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug)
  {
    m_allocator = &allocator;
    m_debug = &debug;
  }

  void destroy()
  {
    if(m_allocator != nullptr)
    {
      m_allocator->destroy(m_outputBuffer);
      m_allocator->destroy(m_sensorBuffer);
    }
    m_outputBuffer = {};
    m_sensorBuffer = {};
    m_outputCapacity = 0;
    m_sensorCapacity = 0;
  }

  bool ensureOutputCapacity(uint64_t elementCount, VkDeviceSize elementSize, const char* debugName)
  {
    if(elementCount == 0 || elementSize == 0 ||
       elementCount > static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / elementSize) ||
       m_allocator == nullptr)
    {
      return false;
    }
    if(m_outputBuffer.buffer != VK_NULL_HANDLE && elementCount <= m_outputCapacity)
    {
      return true;
    }

    m_allocator->destroy(m_outputBuffer);
    const VkDeviceSize outputBytes = static_cast<VkDeviceSize>(elementCount * elementSize);
    m_outputBuffer = m_allocator->createBuffer(outputBytes,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_outputCapacity = m_outputBuffer.buffer != VK_NULL_HANDLE ? elementCount : 0;
    if(m_debug != nullptr && m_outputBuffer.buffer != VK_NULL_HANDLE && debugName != nullptr)
    {
      m_debug->setObjectName(m_outputBuffer.buffer, debugName);
    }
    return m_outputBuffer.buffer != VK_NULL_HANDLE;
  }

  bool ensureSensorCapacity(uint32_t sensorCount, VkDeviceSize sensorSize, const char* debugName)
  {
    if(sensorCount == 0 || sensorSize == 0 ||
       static_cast<uint64_t>(sensorCount) >
           static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / sensorSize) ||
       m_allocator == nullptr)
    {
      return false;
    }
    if(m_sensorBuffer.buffer != VK_NULL_HANDLE && sensorCount <= m_sensorCapacity)
    {
      return true;
    }

    m_allocator->destroy(m_sensorBuffer);
    const VkDeviceSize sensorBytes = static_cast<VkDeviceSize>(sensorCount * sensorSize);
    m_sensorBuffer = m_allocator->createBuffer(sensorBytes,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_sensorCapacity = m_sensorBuffer.buffer != VK_NULL_HANDLE ? sensorCount : 0;
    if(m_debug != nullptr && m_sensorBuffer.buffer != VK_NULL_HANDLE && debugName != nullptr)
    {
      m_debug->setObjectName(m_sensorBuffer.buffer, debugName);
    }
    return m_sensorBuffer.buffer != VK_NULL_HANDLE;
  }

  nvvk::Buffer& outputBuffer() { return m_outputBuffer; }
  const nvvk::Buffer& outputBuffer() const { return m_outputBuffer; }
  nvvk::Buffer& sensorBuffer() { return m_sensorBuffer; }
  const nvvk::Buffer& sensorBuffer() const { return m_sensorBuffer; }
  uint64_t outputCapacity() const { return m_outputCapacity; }
  uint32_t sensorCapacity() const { return m_sensorCapacity; }

private:
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};
  nvvk::Buffer m_outputBuffer;
  nvvk::Buffer m_sensorBuffer;
  uint64_t m_outputCapacity{0};
  uint32_t m_sensorCapacity{0};
};
