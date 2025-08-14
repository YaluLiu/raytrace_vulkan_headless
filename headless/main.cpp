/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "hello_vulkan.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/context_vk.hpp"


//////////////////////////////////////////////////////////////////////////
#define UNUSED(x) (void)(x)
//////////////////////////////////////////////////////////////////////////

// Default search path for shaders
std::vector<std::string> defaultSearchPaths;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static int const SAMPLE_WIDTH  = 1280;
static int const SAMPLE_HEIGHT = 720;


//--------------------------------------------------------------------------------------------------
// Application Entry
//
int main(int argc, char** argv)
{
  // 避免未使用参数警告
  UNUSED(argc);

  // 设置摄像机窗口尺寸
  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  // 设置摄像机初始观察参数（eye, center, up）
  CameraManip.setLookat(glm::vec3(5, 4, -4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

  // 设置一些系统相关的初始化，比如日志等
  NVPSystem system(PROJECT_NAME);

  // 设置shader及media等资源的查找路径
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "..",
      std::string(PROJECT_NAME),
  };

  // 配置Vulkan上下文创建信息（版本、扩展、layer等）
  nvvk::ContextCreateInfo contextInfo;
  contextInfo.setVersion(1, 2);                       // 使用Vulkan 1.2

  // #VKRay: 激活光线追踪相关扩展
  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);  // 加速结构
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);  // 光追管线
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);  // 光追依赖

  // 创建Vulkan应用上下文
  nvvk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  // 查询所有兼容设备
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  // 初始化设备（选第一个兼容的）
  vkctx.initDevice(compatibleDevices[0], contextInfo);

  // 创建HelloVulkan示例对象
  HelloVulkan helloVk;

  // 准备创建信息结构体
  nvvkhl::AppBaseVkCreateInfo createInfo{};
  createInfo.instance = vkctx.m_instance;
  createInfo.device = vkctx.m_device;
  createInfo.physicalDevice = vkctx.m_physicalDevice;
  createInfo.queueIndices = {vkctx.m_queueGCT.familyIndex}; // 使用图形队列家族索引
  createInfo.size = {SAMPLE_WIDTH, SAMPLE_HEIGHT}; // 设置窗口尺寸

  // 使用单一create函数初始化所有内容
  helloVk.create(createInfo);

  // 加载场景模型
  // 加载地面
  helloVk.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true),
                    glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));
  // 加载wuson模型
  helloVk.loadModel(nvh::findFile("media/scenes/wuson.obj", defaultSearchPaths, true));
  // 添加多个wuson实例
  uint32_t  wusonId = 1;
  glm::mat4 identity{1};
  for(int i = 0; i < 5; i++)
  {
    helloVk.m_instances.push_back({identity, wusonId});
  }
  // 加载球体模型
  helloVk.loadModel(nvh::findFile("media/scenes/sphere.obj", defaultSearchPaths, true));

  // 创建离屏渲染缓冲
  helloVk.createOffscreenRender();
  // 创建描述符布局
  helloVk.createDescriptorSetLayout();
  // 创建图形管线
  helloVk.createGraphicsPipeline();
  // 创建全局uniform缓冲
  helloVk.createUniformBuffer();
  // 创建场景描述缓冲
  helloVk.createObjDescriptionBuffer();
  // 更新描述符集
  helloVk.updateDescriptorSet();

  // #VKRay 光线追踪相关初始化
  helloVk.initRayTracing();
  helloVk.createBottomLevelAS();
  helloVk.createTopLevelAS();
  helloVk.createRtDescriptorSet();
  helloVk.createRtPipeline();

  // #VK_compute 计算着色器相关描述符与管线
  helloVk.createCompDescriptors();
  helloVk.createCompPipelines();

  // 设置默认清屏色
  glm::vec4 clearColor   = glm::vec4(1, 1, 1, 1.00f);
  // 记录程序启动时间
  auto      start        = std::chrono::system_clock::now();

  // 进入主循环
  int frame_idx = 0;
  while(true)
  {
    // #VK_animation 动画相关
    std::chrono::duration<float> diff = std::chrono::system_clock::now() - start;
    // 更新球体动画
    helloVk.animationObject(diff.count());
    // 更新wuson实例动画
    helloVk.animationInstances(diff.count());

    // 获取当前帧的索引
    auto                   curFrame = helloVk.getCurFrame();
    // 获取本帧用的命令缓冲
    const VkCommandBuffer& cmdBuf   = helloVk.getCommandBuffers()[curFrame];

    // 开始记录命令缓冲
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // 更新摄像机uniform缓冲
    helloVk.updateUniformBuffer(cmdBuf);

    // 清屏值（颜色和深度）
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};
    
    // 光线追踪渲染
    helloVk.raytrace(cmdBuf, clearColor);

    // 提交本帧渲染命令
    vkEndCommandBuffer(cmdBuf);
    helloVk.submitFrame();

    helloVk.saveOffscreenColorToFile("headless.png");
    if(++frame_idx > 10)
      break;
  }

  // 退出前等待设备空闲
  vkDeviceWaitIdle(helloVk.getDevice());

  // 销毁所有资源
  helloVk.destroyResources();
  helloVk.destroy();
  vkctx.deinit();

  // 正常退出
  return 0;
}