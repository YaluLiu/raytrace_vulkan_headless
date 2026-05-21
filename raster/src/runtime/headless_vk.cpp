#include "runtime/headless_vk.hpp"
#include "nvp/perproject_globals.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include "imgui/imgui_camera_widget.h"
#include "imgui/imgui_helper.h"
#include <iostream>

void nvvkhl::AppOffline::create(const AppBaseVkCreateInfo& info)
{
  setup(info.instance, info.device, info.physicalDevice, info.queueIndices[0]);
  createCommandBuffers();
  m_size = info.size;
}

void nvvkhl::AppOffline::setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex)
{
  m_instance       = instance;
  m_device         = device;
  m_physicalDevice = physicalDevice;
  m_graphicsQueueIndex = graphicsQueueIndex;
  vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_queue);

  VkCommandPoolCreateInfo poolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_cmdPool);

  VkPipelineCacheCreateInfo pipelineCacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  vkCreatePipelineCache(m_device, &pipelineCacheInfo, nullptr, &m_pipelineCache);
}

void nvvkhl::AppOffline::destroy()
{
  vkDeviceWaitIdle(m_device);

  vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

  for(uint32_t i = 0; i < m_imageCount; i++)
  {
    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_commandBuffers[i]);
  }
  vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
}

void nvvkhl::AppOffline::createCommandBuffers()
{
  VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocateInfo.commandPool        = m_cmdPool;
  allocateInfo.commandBufferCount = m_imageCount;
  allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  m_commandBuffers.resize(m_imageCount);
  vkAllocateCommandBuffers(m_device, &allocateInfo, m_commandBuffers.data());

#ifndef NDEBUG
  for(size_t i = 0; i < m_commandBuffers.size(); i++)
  {
    std::string name = std::string("AppBase") + std::to_string(i);

    VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    nameInfo.objectHandle = (uint64_t)m_commandBuffers[i];
    nameInfo.objectType   = VK_OBJECT_TYPE_COMMAND_BUFFER;
    nameInfo.pObjectName  = name.c_str();
    vkSetDebugUtilsObjectNameEXT(m_device, &nameInfo);
  }
#endif
}

void nvvkhl::AppOffline::submitFrame()
{
  const VkCommandBuffer& cmdBuf = m_commandBuffers[m_imageIndex];

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuf;

  vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(m_queue);
}

uint32_t nvvkhl::AppOffline::getMemoryType(uint32_t typeBits, const VkMemoryPropertyFlags& properties) const
{
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

  for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
  {
    if(((typeBits & (1 << i)) > 0) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
      return i;
  }
  LOGE("Unable to find memory type %u\n", static_cast<unsigned int>(properties));
  assert(0);
  return ~0u;
}

VkCommandBuffer nvvkhl::AppOffline::createTempCmdBuffer()
{
  VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocateInfo.commandBufferCount = 1;
  allocateInfo.commandPool        = m_cmdPool;
  allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  VkCommandBuffer cmdBuffer;
  vkAllocateCommandBuffers(m_device, &allocateInfo, &cmdBuffer);

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuffer, &beginInfo);
  return cmdBuffer;
}

void nvvkhl::AppOffline::submitTempCmdBuffer(VkCommandBuffer cmdBuffer)
{
  vkEndCommandBuffer(cmdBuffer);

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuffer;
  vkQueueSubmit(m_queue, 1, &submitInfo, {});
  vkQueueWaitIdle(m_queue);
  vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmdBuffer);
}
