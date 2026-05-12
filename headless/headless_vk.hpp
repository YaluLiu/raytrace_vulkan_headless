#pragma once

#include "vulkan/vulkan_core.h"

#ifdef LINUX
#include <unistd.h>
#endif

#include <vector>

namespace nvvkhl {

struct AppBaseVkCreateInfo
{
  VkInstance            instance{};
  VkDevice              device{};
  VkPhysicalDevice      physicalDevice{};
  std::vector<uint32_t> queueIndices{};
  VkExtent2D            size{};
};

class AppOffline
{
public:
  AppOffline()          = default;
  virtual ~AppOffline() = default;

  virtual void create(const AppBaseVkCreateInfo& info);
  virtual void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex);
  virtual void destroy();

  virtual void createCommandBuffers();
  virtual void submitFrame();

  VkInstance                          getInstance() { return m_instance; }
  VkDevice                            getDevice() { return m_device; }
  VkPhysicalDevice                    getPhysicalDevice() { return m_physicalDevice; }
  VkQueue                             getQueue() { return m_queue; }
  uint32_t                            getQueueFamily() { return m_graphicsQueueIndex; }
  VkCommandPool                       getCommandPool() { return m_cmdPool; }
  VkPipelineCache                     getPipelineCache() { return m_pipelineCache; }
  const std::vector<VkCommandBuffer>& getCommandBuffers() { return m_commandBuffers; }
  uint32_t                            getCurFrame() const { return m_imageIndex; }

protected:
  VkInstance       m_instance{};
  VkDevice         m_device{};
  VkPhysicalDevice m_physicalDevice{};
  VkQueue          m_queue{VK_NULL_HANDLE};
  uint32_t         m_graphicsQueueIndex{VK_QUEUE_FAMILY_IGNORED};
  VkCommandPool    m_cmdPool{VK_NULL_HANDLE};

  std::vector<VkCommandBuffer> m_commandBuffers;
  std::vector<VkFence>         m_waitFences;
  VkPipelineCache              m_pipelineCache{VK_NULL_HANDLE};

  VkExtent2D m_size;
  uint32_t m_imageIndex = 0;
  uint32_t m_imageCount = 1;

  uint32_t        getMemoryType(uint32_t typeBits, const VkMemoryPropertyFlags& properties) const;
  VkCommandBuffer createTempCmdBuffer();
  void            submitTempCmdBuffer(VkCommandBuffer cmdBuffer);
};

}
