/*
 * Copyright (c) 2021-2024, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION.
 * SPDX-License-Identifier: Apache-2.0
 */

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

  // Getters
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
  // Vulkan low level
  VkInstance       m_instance{};
  VkDevice         m_device{};
  VkPhysicalDevice m_physicalDevice{};
  VkQueue          m_queue{VK_NULL_HANDLE};
  uint32_t         m_graphicsQueueIndex{VK_QUEUE_FAMILY_IGNORED};
  VkCommandPool    m_cmdPool{VK_NULL_HANDLE};

  // Drawing/Surface
  std::vector<VkCommandBuffer> m_commandBuffers;                 // Command buffer per nb element in Swapchain
  std::vector<VkFence>         m_waitFences;                     // Fences per nb element in Swapchain
  VkPipelineCache              m_pipelineCache{VK_NULL_HANDLE};  // Cache for pipeline/shaders

  // image size
  VkExtent2D m_size;
  // cmd-buffer-size
  uint32_t m_imageIndex = 0;
  uint32_t m_imageCount = 1;

  // for save color_image to local png file
  uint32_t        getMemoryType(uint32_t typeBits, const VkMemoryPropertyFlags& properties) const;
  VkCommandBuffer createTempCmdBuffer();
  void            submitTempCmdBuffer(VkCommandBuffer cmdBuffer);
};

}  // namespace nvvkhl
