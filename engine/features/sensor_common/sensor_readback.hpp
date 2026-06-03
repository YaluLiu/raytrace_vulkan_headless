#pragma once

#include "nvvk/commands_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

#include <functional>

#include <vulkan/vulkan_core.h>

inline bool ReadDeviceBuffer(VkDevice device,
                             uint32_t graphicsQueueIndex,
                             nvvk::ResourceAllocatorDma& allocator,
                             VkBuffer source,
                             VkDeviceSize byteCount,
                             VkPipelineStageFlags sourceStage,
                             VkAccessFlags sourceAccess,
                             const std::function<void(const void*)>& consumeMappedBytes)
{
  if(device == VK_NULL_HANDLE || source == VK_NULL_HANDLE || byteCount == 0)
  {
    return false;
  }

  nvvk::Buffer stagingBuffer =
      allocator.createBuffer(byteCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if(stagingBuffer.buffer == VK_NULL_HANDLE)
  {
    return false;
  }

  {
    nvvk::CommandPool commandPool(device, graphicsQueueIndex);
    VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
    VkBufferMemoryBarrier beforeCopy{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    beforeCopy.srcAccessMask = sourceAccess;
    beforeCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    beforeCopy.buffer = source;
    beforeCopy.offset = 0;
    beforeCopy.size = byteCount;
    vkCmdPipelineBarrier(cmdBuf, sourceStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &beforeCopy, 0,
                         nullptr);

    VkBufferCopy copyRegion{0, 0, byteCount};
    vkCmdCopyBuffer(cmdBuf, source, stagingBuffer.buffer, 1, &copyRegion);
    commandPool.submitAndWait(cmdBuf);
  }

  const void* mapped = allocator.map(stagingBuffer);
  if(mapped == nullptr)
  {
    allocator.destroy(stagingBuffer);
    return false;
  }

  consumeMappedBytes(mapped);

  allocator.unmap(stagingBuffer);
  allocator.destroy(stagingBuffer);
  return true;
}
