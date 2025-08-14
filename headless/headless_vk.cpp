/*
 * Copyright (c) 2021-2025, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025, NVIDIA CORPORATION.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "headless_vk.hpp"
#include "nvp/perproject_globals.hpp"
// Imgui
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include "imgui/imgui_camera_widget.h"
#include "imgui/imgui_helper.h"

//--------------------------------------------------------------------------------------------------
// 创建应用的所有元素的顺序
// 首先将 Vulkan 的实例、设备等保存为类成员
// 然后创建交换链、深度缓冲区、默认渲染通道以及
// 交换链的帧缓冲（所有帧缓冲共享同一个深度图像）
// 初始化 Imgui 并设置窗口操作的回调函数（鼠标、键盘等）
void nvvkhl::AppOffline::create(const AppBaseVkCreateInfo& info)
{
  // 初始化 Vulkan 相关的实例、设备、物理设备、队列等
  setup(info.instance, info.device, info.physicalDevice, info.queueIndices[0]);
  // 创建命令命令缓冲区
  createCommandBuffers();
  m_size = info.size;
}

//--------------------------------------------------------------------------------------------------
// 配置底层 Vulkan，用于各种操作
// 中文注释：初始化 Vulkan 基础对象，包括队列、命令池、管线缓存
void nvvkhl::AppOffline::setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex)
{
  // 保存 Vulkan 实例、逻辑设备、物理设备
  m_instance           = instance;
  m_device             = device;
  m_physicalDevice     = physicalDevice;
  // 保存图形队列的 family index
  m_graphicsQueueIndex = graphicsQueueIndex;
  // 获取队列（index=0），存入 m_queue
  vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_queue);

  // 创建命令池（支持重置命令缓冲）
  VkCommandPoolCreateInfo poolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_cmdPool);

  // 创建管线缓存（可选，提升管线创建效率）
  VkPipelineCacheCreateInfo pipelineCacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  vkCreatePipelineCache(m_device, &pipelineCacheInfo, nullptr, &m_pipelineCache);
}

//--------------------------------------------------------------------------------------------------
// 程序退出时调用，销毁所有 Vulkan 资源
// 中文注释：销毁所有 Vulkan 相关对象、释放内存、清理 ImGui
void nvvkhl::AppOffline::destroy()
{
  // 等待设备空闲，确保没有任务正在运行
  vkDeviceWaitIdle(m_device);

  // 销毁管线缓存
  vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

  // 遍历交换链中的每一帧，销毁相关资源
  for(uint32_t i = 0; i < m_imageCount; i++)
  {
    // // 销毁等待信号量（Fence）
    // vkDestroyFence(m_device, m_waitFences[i], nullptr);

    // 释放命令缓冲
    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_commandBuffers[i]);
  }
  // 销毁命令池
  vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
}

void nvvkhl::AppOffline::createCommandBuffers() 
{
  // 为每个frame分配一个命令缓冲区
  VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocateInfo.commandPool        = m_cmdPool;
  allocateInfo.commandBufferCount = m_imageCount;
  allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  m_commandBuffers.resize(m_imageCount);
  vkAllocateCommandBuffers(m_device, &allocateInfo, m_commandBuffers.data());
  
  // // 为每一帧（每个swapchain image）创建一个Fence用于同步
  // m_waitFences.resize(m_imageCount);
  // for(auto& fence : m_waitFences)
  // {
  //   VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  //   fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  //   vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence);
  // }

#ifndef NDEBUG
  // 给每个命令缓冲区设置调试名字
  for(size_t i = 0; i < m_commandBuffers.size(); i++)
  {
    std::string name = std::string("AppBase") + std::to_string(i);

    VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    nameInfo.objectHandle = (uint64_t)m_commandBuffers[i];
    nameInfo.objectType   = VK_OBJECT_TYPE_COMMAND_BUFFER;
    nameInfo.pObjectName  = name.c_str();
    vkSetDebugUtilsObjectNameEXT(m_device, &nameInfo);
  }
#endif  // !NDEBUG
}

//--------------------------------------------------------------------------------------------------
// 每帧渲染命令录制结束后调用，提交当前帧命令缓冲到图形队列并处理同步和Present
// 核心流程：
// - 重置Fence
// - 设置多卡（NVLINK）支持参数（如启用）
// - 填写submit和present参数，提交队列
// - Present本帧swapchain image
void nvvkhl::AppOffline::submitFrame()
{
  // 只提交第一个（或你实际用的）命令缓冲
  const VkCommandBuffer& cmdBuf = m_commandBuffers[m_imageIndex];

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuf;

  vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);

  // 等待队列完成（保证输出image可读取）
  vkQueueWaitIdle(m_queue);
}

//--------------------------------------------------------------------------------------------------
// 工具函数：根据需求查找合适的内存类型（如device local等）
// - typeBits为类型掩码（来自vkGet*MemoryRequirements）
// - properties为所需属性（如VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT）
// - 返回可用内存类型索引
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

//--------------------------------------------------------------------------------------------------
// 创建一个临时命令缓冲区（一次性提交用）
// - 通常用于资源创建、布局转换等短命令
// 保存图片时需要用到
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

//--------------------------------------------------------------------------------------------------
// 提交并释放临时命令缓冲区
// - 通常与createTempCmdBuffer配合使用
// - 用于一次性的GPU操作，如资源上传、布局切换等
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