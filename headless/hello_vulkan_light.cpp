#include <sstream>
#include "hello_vulkan.hpp"
#include "nvh/alignment.hpp"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "nvvk/buffers_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;

void HelloVulkan::createRtDescriptorSet()
{
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // NEW: objectId image
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eObjIdImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

  m_rtDescPool      = m_rtDescSetLayoutBind.createPool(m_device);
  m_rtDescSetLayout = m_rtDescSetLayoutBind.createLayout(m_device);

  VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocateInfo.descriptorPool     = m_rtDescPool;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts        = &m_rtDescSetLayout;
  vkAllocateDescriptorSets(m_device, &allocateInfo, &m_rtDescSet);

  VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
  VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
  descASInfo.accelerationStructureCount = 1;
  descASInfo.pAccelerationStructures    = &tlas;
  VkDescriptorImageInfo imageInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo idInfo{{}, m_offscreenObjectId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eTlas, &descASInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &imageInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eObjIdImage, &idInfo));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void HelloVulkan::updateRtDescriptorSet()
{
  VkDescriptorImageInfo colorInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo idInfo{{}, m_offscreenObjectId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  VkWriteDescriptorSet wColor = m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &colorInfo);
  VkWriteDescriptorSet wId    = m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eObjIdImage, &idInfo);
  VkWriteDescriptorSet ws[2]  = {wColor, wId};
  vkUpdateDescriptorSets(m_device, 2, ws, 0, nullptr);
}

void HelloVulkan::createRtPipeline()
{
  enum StageIndices
  {
    eRaygen,
    eMiss,
    eMiss2,
    eClosestHit,
    eShaderGroupCount
  };

  std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
  VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stage.pName = "main";  // 所有shader入口均为main

  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rgen.spv", true, defaultSearchPaths, true));
  stage.stage     = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  stages[eRaygen] = stage;

  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage   = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss] = stage;

  stage.module =
      nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytraceShadow.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage    = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss2] = stage;

  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rchit.spv", true, defaultSearchPaths, true));
  stage.stage         = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  stages[eClosestHit] = stage;

  VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
  group.anyHitShader       = VK_SHADER_UNUSED_KHR;
  group.closestHitShader   = VK_SHADER_UNUSED_KHR;
  group.generalShader      = VK_SHADER_UNUSED_KHR;
  group.intersectionShader = VK_SHADER_UNUSED_KHR;

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eRaygen;
  m_rtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss;
  m_rtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss2;
  m_rtShaderGroups.push_back(group);

  group.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  group.generalShader    = VK_SHADER_UNUSED_KHR;
  group.closestHitShader = eClosestHit;
  m_rtShaderGroups.push_back(group);


  VkPushConstantRange pushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                                   0, sizeof(PushConstantRay)};


  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;


  std::vector<VkDescriptorSetLayout> rtDescSetLayouts = {m_rtDescSetLayout, m_descSetLayout};
  pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(rtDescSetLayouts.size());
  pipelineLayoutCreateInfo.pSetLayouts                = rtDescSetLayouts.data();

  vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_rtPipelineLayout);

  VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
  rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
  rayPipelineInfo.pStages    = stages.data();
  rayPipelineInfo.groupCount = static_cast<uint32_t>(m_rtShaderGroups.size());
  rayPipelineInfo.pGroups    = m_rtShaderGroups.data();

  rayPipelineInfo.maxPipelineRayRecursionDepth = 2;
  rayPipelineInfo.layout                       = m_rtPipelineLayout;

  vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_rtPipeline);

  m_sbtWrapper.create(m_rtPipeline, rayPipelineInfo);

  for(auto& s : stages)
    vkDestroyShaderModule(m_device, s.module, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 执行光线追踪渲染主流程（生成光追图像到offscreen image）
void HelloVulkan::raytrace(const VkCommandBuffer& cmdBuf)
{
  m_debug.beginLabel(cmdBuf, "Ray trace");
  m_pcRay.numLights = static_cast<int>(m_lights.size());  // 新增
  std::vector<VkDescriptorSet> descSets{m_rtDescSet, m_descSet};
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0,
                          (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
  vkCmdPushConstants(cmdBuf, m_rtPipelineLayout,
                     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                     0, sizeof(PushConstantRay), &m_pcRay);

  auto& regions = m_sbtWrapper.getRegions();
  vkCmdTraceRaysKHR(cmdBuf, &regions[0], &regions[1], &regions[2], &regions[3], m_size.width, m_size.height, 1);

  m_debug.endLabel(cmdBuf);
}

//--------------------------------------------------------------------------------------------------
// 创建图形渲染用的描述符集布局
// 包括：全局UBO，物体描述buffer，所有纹理采样器
void HelloVulkan::createDescriptorSetLayout()
{
  auto nbTxt = static_cast<uint32_t>(m_textures.size());

  // 摄像机矩阵 UBO
  m_descSetLayoutBind.addBinding(SceneBindings::eGlobals, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // 物体描述 SSBO
  m_descSetLayoutBind.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // 所有纹理采样器
  m_descSetLayoutBind.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTxt,
                                 VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // 添加灯光缓冲区绑定
  m_descSetLayoutBind.addBinding(SceneBindings::eLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // 新增：实例ID缓冲区绑定
  m_descSetLayoutBind.addBinding(SceneBindings::eInstanceIds, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // 创建layout和pool
  m_descSetLayout = m_descSetLayoutBind.createLayout(m_device);
  m_descPool      = m_descSetLayoutBind.createPool(m_device, 1);
  m_descSet       = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

//--------------------------------------------------------------------------------------------------
// 用实际的buffer/image填充描述符集
void HelloVulkan::updateDescriptorSet()
{
  std::vector<VkWriteDescriptorSet> writes;

  // 摄像机矩阵 UBO
  VkDescriptorBufferInfo dbiUnif{m_bGlobals.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eGlobals, &dbiUnif));

  // 物体描述 SSBO
  VkDescriptorBufferInfo dbiSceneDesc{m_bObjDesc.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eObjDescs, &dbiSceneDesc));

  // 纹理数组
  std::vector<VkDescriptorImageInfo> diit;
  for(auto& texture : m_textures)
  {
    diit.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_descSetLayoutBind.makeWriteArray(m_descSet, SceneBindings::eTextures, diit.data()));
  // 添加灯光缓冲区描述符更新
  VkDescriptorBufferInfo dbiLights{m_bLights.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, eLights, &dbiLights));
  // 实例ID缓冲区描述符更新
  VkDescriptorBufferInfo dbiInstanceIds{m_bInstanceIds.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eInstanceIds, &dbiInstanceIds));
  // 写入描述符集
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 灯光管理函数
//
void HelloVulkan::addLight(const Light& light)
{
  m_lights.push_back(light);
}

void HelloVulkan::clearLights()
{
  m_lights.clear();
}

void HelloVulkan::createLightBuffer()
{
  // 创建灯光缓冲区（初始支持100个灯光）
  size_t maxLights  = 100;
  size_t bufferSize = sizeof(Light) * maxLights;

  m_bLights = m_alloc.createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug.setObjectName(m_bLights.buffer, "Lights");
}

void HelloVulkan::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  if(!m_lights.empty())
  {
    vkCmdUpdateBuffer(cmdBuf, m_bLights.buffer, 0, sizeof(Light) * m_lights.size(), m_lights.data());
  }
}

void HelloVulkan::createInstanceIdBuffer()
{
  size_t maxInstances = std::max(size_t(1000), m_instanceIds.size() * 2);  // 预留空间
  size_t bufferSize   = sizeof(int) * maxInstances;

  m_bInstanceIds = m_alloc.createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug.setObjectName(m_bInstanceIds.buffer, "InstanceIds");
}

void HelloVulkan::updateInstanceIdBuffer(const VkCommandBuffer& cmdBuf)
{
  if(!m_instanceIds.empty())
  {
    vkCmdUpdateBuffer(cmdBuf, m_bInstanceIds.buffer, 0, sizeof(int) * m_instanceIds.size(), m_instanceIds.data());
  }
}
