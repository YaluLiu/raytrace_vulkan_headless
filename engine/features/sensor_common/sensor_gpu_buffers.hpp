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
      m_allocator->destroy(m_generatedSamplesBuffer);
      m_allocator->destroy(m_sensorMetadataBuffer);
    }
    m_generatedSamplesBuffer = {};
    m_sensorMetadataBuffer = {};
    m_generatedSampleCapacity = 0;
    m_sensorMetadataCapacity = 0;
  }

  bool ensureGeneratedSampleCapacity(uint64_t elementCount, VkDeviceSize elementSize, const char* debugName)
  {
    if(elementCount == 0 || elementSize == 0 ||
       elementCount > static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / elementSize) ||
       m_allocator == nullptr)
    {
      return false;
    }
    if(m_generatedSamplesBuffer.buffer != VK_NULL_HANDLE && elementCount <= m_generatedSampleCapacity)
    {
      return true;
    }

    m_allocator->destroy(m_generatedSamplesBuffer);
    const VkDeviceSize generatedSampleBytes = static_cast<VkDeviceSize>(elementCount * elementSize);
    m_generatedSamplesBuffer = m_allocator->createBuffer(generatedSampleBytes,
                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_generatedSampleCapacity = m_generatedSamplesBuffer.buffer != VK_NULL_HANDLE ? elementCount : 0;
    if(m_debug != nullptr && m_generatedSamplesBuffer.buffer != VK_NULL_HANDLE && debugName != nullptr)
    {
      m_debug->setObjectName(m_generatedSamplesBuffer.buffer, debugName);
    }
    return m_generatedSamplesBuffer.buffer != VK_NULL_HANDLE;
  }

  bool ensureSensorMetadataCapacity(uint32_t sensorCount, VkDeviceSize sensorSize, const char* debugName)
  {
    if(sensorCount == 0 || sensorSize == 0 ||
       static_cast<uint64_t>(sensorCount) >
           static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / sensorSize) ||
       m_allocator == nullptr)
    {
      return false;
    }
    if(m_sensorMetadataBuffer.buffer != VK_NULL_HANDLE && sensorCount <= m_sensorMetadataCapacity)
    {
      return true;
    }

    m_allocator->destroy(m_sensorMetadataBuffer);
    const VkDeviceSize sensorMetadataBytes = static_cast<VkDeviceSize>(sensorCount * sensorSize);
    m_sensorMetadataBuffer = m_allocator->createBuffer(sensorMetadataBytes,
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_sensorMetadataCapacity = m_sensorMetadataBuffer.buffer != VK_NULL_HANDLE ? sensorCount : 0;
    if(m_debug != nullptr && m_sensorMetadataBuffer.buffer != VK_NULL_HANDLE && debugName != nullptr)
    {
      m_debug->setObjectName(m_sensorMetadataBuffer.buffer, debugName);
    }
    return m_sensorMetadataBuffer.buffer != VK_NULL_HANDLE;
  }

  nvvk::Buffer& generatedSamplesBuffer() { return m_generatedSamplesBuffer; }
  const nvvk::Buffer& generatedSamplesBuffer() const { return m_generatedSamplesBuffer; }
  nvvk::Buffer& sensorMetadataBuffer() { return m_sensorMetadataBuffer; }
  const nvvk::Buffer& sensorMetadataBuffer() const { return m_sensorMetadataBuffer; }
  uint64_t generatedSampleCapacity() const { return m_generatedSampleCapacity; }
  uint32_t sensorMetadataCapacity() const { return m_sensorMetadataCapacity; }

private:
  nvvk::ResourceAllocatorDma* m_allocator{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};
  nvvk::Buffer m_generatedSamplesBuffer;
  nvvk::Buffer m_sensorMetadataBuffer;
  uint64_t m_generatedSampleCapacity{0};
  uint32_t m_sensorMetadataCapacity{0};
};
