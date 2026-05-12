#include <sstream>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <stdexcept>
#include "hello_vulkan.hpp"
#include "hello_vulkan_barriers.hpp"
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

namespace {
float haltonComponent(uint32_t index, uint32_t base)
{
  float result = 0.0f;
  float invBase = 1.0f / static_cast<float>(base);
  float fraction = invBase;

  while(index > 0)
  {
    result += static_cast<float>(index % base) * fraction;
    index /= base;
    fraction *= invBase;
  }

  return result;
}

glm::vec2 computeTemporalJitter(uint32_t frameIndex)
{
  const uint32_t haltonIndex = (frameIndex % 1024u) + 1u;
  return glm::vec2(haltonComponent(haltonIndex, 2u), haltonComponent(haltonIndex, 3u)) - glm::vec2(0.5f);
}

}

void HelloVulkan::createRtDescriptorSet()
{
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eObjIdImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eInsIdImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eLidarPointCloudImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eDepthImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR);

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
  VkDescriptorImageInfo InstanceIdInfo{{}, m_offscreenInstanceId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo depthAovInfo{{}, m_offscreenDepthAov.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo lidarPointCloudInfo{{}, m_offscreenLidarPointCloud.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eTlas, &descASInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &imageInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eObjIdImage, &idInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eInsIdImage, &InstanceIdInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eLidarPointCloudImage, &lidarPointCloudInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eDepthImage, &depthAovInfo));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void HelloVulkan::updateRtDescriptorSet()
{
  VkDescriptorImageInfo colorInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo idInfo{{}, m_offscreenObjectId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo InsIdInfo{{}, m_offscreenInstanceId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo depthAovInfo{{}, m_offscreenDepthAov.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo lidarPointCloudInfo{{}, m_offscreenLidarPointCloud.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &colorInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eObjIdImage, &idInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eInsIdImage, &InsIdInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eLidarPointCloudImage, &lidarPointCloudInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eDepthImage, &depthAovInfo));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void HelloVulkan::createRtPipeline()
{
  enum StageIndices
  {
    eRaygen,
    eMiss,
    eMiss2,
    eClosestHit,
    eClosestHit2,
    eIntersection,
    eShaderGroupCount
  };

  std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
  VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stage.pName = "main";
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rgen.spv", true, defaultSearchPaths, true));
  stage.stage     = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  stages[eRaygen] = stage;
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage   = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss] = stage;
  // The second miss shader is invoked when a shadow ray misses the geometry. It simply indicates that no occlusion has been found
  stage.module =
      nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytraceShadow.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage    = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss2] = stage;
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rchit.spv", true, defaultSearchPaths, true));
  stage.stage         = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  stages[eClosestHit] = stage;
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace2.rchit.spv", true, defaultSearchPaths, true));
  stage.stage          = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  stages[eClosestHit2] = stage;
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rint.spv", true, defaultSearchPaths, true));
  stage.stage           = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
  stages[eIntersection] = stage;

  VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
  group.anyHitShader       = VK_SHADER_UNUSED_KHR;
  group.closestHitShader   = VK_SHADER_UNUSED_KHR;
  group.generalShader      = VK_SHADER_UNUSED_KHR;
  group.intersectionShader = VK_SHADER_UNUSED_KHR;

  m_rtShaderGroups.clear();

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

  group.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
  group.closestHitShader   = eClosestHit2;
  group.intersectionShader = eIntersection;
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

  uint32_t desiredRecursionDepth               = 8;
  rayPipelineInfo.maxPipelineRayRecursionDepth = std::min(desiredRecursionDepth, m_rtProperties.maxRayRecursionDepth);
  rayPipelineInfo.layout                       = m_rtPipelineLayout;

  vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_rtPipeline);

  m_sbtWrapper.create(m_rtPipeline, rayPipelineInfo);

  for(auto& s : stages)
    vkDestroyShaderModule(m_device, s.module, nullptr);
}


void HelloVulkan::createRtShaderBindingTable()
{
  uint32_t missCount{2};
  uint32_t hitCount{2};
  auto     handleCount = 1 + missCount + hitCount;
  uint32_t handleSize  = m_rtProperties.shaderGroupHandleSize;

  // The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
  uint32_t handleSizeAligned = nvh::align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

  m_rgenRegion.stride = nvh::align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_rgenRegion.size = m_rgenRegion.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
  m_missRegion.stride = handleSizeAligned;
  m_missRegion.size   = nvh::align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_hitRegion.stride  = handleSizeAligned;
  m_hitRegion.size    = nvh::align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

  uint32_t             dataSize = handleCount * handleSize;
  std::vector<uint8_t> handles(dataSize);
  auto result = vkGetRayTracingShaderGroupHandlesKHR(m_device, m_rtPipeline, 0, handleCount, dataSize, handles.data());
  assert(result == VK_SUCCESS);

  VkDeviceSize sbtSize = m_rgenRegion.size + m_missRegion.size + m_hitRegion.size + m_callRegion.size;
  m_rtSBTBuffer        = m_alloc.createBuffer(sbtSize,
                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                  | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_debug.setObjectName(m_rtSBTBuffer.buffer, std::string("SBT"));

  VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_rtSBTBuffer.buffer};
  VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(m_device, &info);
  m_rgenRegion.deviceAddress           = sbtAddress;
  m_missRegion.deviceAddress           = sbtAddress + m_rgenRegion.size;
  m_hitRegion.deviceAddress            = sbtAddress + m_rgenRegion.size + m_missRegion.size;

  auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

  auto*    pSBTBuffer = reinterpret_cast<uint8_t*>(m_alloc.map(m_rtSBTBuffer));
  uint8_t* pData{nullptr};
  uint32_t handleIdx{0};
  pData = pSBTBuffer;
  memcpy(pData, getHandle(handleIdx++), handleSize);
  pData = pSBTBuffer + m_rgenRegion.size;
  for(uint32_t c = 0; c < missCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_missRegion.stride;
  }
  pData = pSBTBuffer + m_rgenRegion.size + m_missRegion.size;
  for(uint32_t c = 0; c < hitCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_hitRegion.stride;
  }

  m_alloc.unmap(m_rtSBTBuffer);
  m_alloc.finalizeAndReleaseStaging();
}

void HelloVulkan::raytrace(const VkCommandBuffer& cmdBuf)
{
  m_debug.beginLabel(cmdBuf, "Ray trace");
  m_pcRay.numLights       = static_cast<int>(m_lights.size());
  m_pcRay.frameIndex      = m_frameIndex;
  m_currentJitter         = computeTemporalJitter(m_frameIndex);
  m_pcRay.jitterX         = m_currentJitter.x;
  m_pcRay.jitterY         = m_currentJitter.y;
  m_pcRay.maxDepth        = std::max(m_pcRay.maxDepth, 1);
  m_pcRay.samplesPerFrame = std::max(m_pcRay.samplesPerFrame, 1);
  m_pcRay.lidarPassMode   = eRaygenPassLowResBeauty;
  std::vector<VkDescriptorSet> descSets{m_rtDescSet, m_descSet};
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0,
                          (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
  const VkShaderStageFlags pushStages =
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

  auto& regions = m_sbtWrapper.getRegions();

  PushConstantRay lowResPassPc = m_pcRay;
  lowResPassPc.lidarPassMode   = eRaygenPassLowResBeauty;
  vkCmdPushConstants(cmdBuf, m_rtPipelineLayout, pushStages, 0, sizeof(PushConstantRay), &lowResPassPc);
  vkCmdTraceRaysKHR(cmdBuf, &regions[0], &regions[1], &regions[2], &regions[3], m_renderSize.width, m_renderSize.height, 1);

  PushConstantRay aovPassPc = m_pcRay;
  aovPassPc.lidarPassMode   = eRaygenPassHighResAov;
  aovPassPc.numLights       = 0;
  aovPassPc.maxDepth        = 1;
  aovPassPc.samplesPerFrame = 1;
  aovPassPc.jitterX         = 0.0f;
  aovPassPc.jitterY         = 0.0f;
  vkCmdPushConstants(cmdBuf, m_rtPipelineLayout, pushStages, 0, sizeof(PushConstantRay), &aovPassPc);
  vkCmdTraceRaysKHR(cmdBuf, &regions[0], &regions[1], &regions[2], &regions[3], m_aovSize.width, m_aovSize.height, 1);

  m_accumulatedFrames++;
  m_frameIndex++;

  m_debug.endLabel(cmdBuf);
}

void HelloVulkan::destroyRasterFramebuffer()
{
  if(m_rasterFramebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(m_device, m_rasterFramebuffer, nullptr);
    m_rasterFramebuffer = VK_NULL_HANDLE;
  }
}

void HelloVulkan::createRasterFramebuffer()
{
  if(m_rasterRenderPass == VK_NULL_HANDLE || m_offscreenColor.descriptor.imageView == VK_NULL_HANDLE
     || m_offscreenObjectId.descriptor.imageView == VK_NULL_HANDLE
     || m_offscreenInstanceId.descriptor.imageView == VK_NULL_HANDLE
     || m_offscreenDepthAov.descriptor.imageView == VK_NULL_HANDLE
     || m_offscreenDepth.descriptor.imageView == VK_NULL_HANDLE)
  {
    return;
  }

  destroyRasterFramebuffer();

  std::array<VkImageView, 5> attachments{
      m_offscreenColor.descriptor.imageView, m_offscreenObjectId.descriptor.imageView,
      m_offscreenInstanceId.descriptor.imageView, m_offscreenDepthAov.descriptor.imageView,
      m_offscreenDepth.descriptor.imageView};

  VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebufferInfo.renderPass      = m_rasterRenderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments    = attachments.data();
  framebufferInfo.width           = m_aovSize.width;
  framebufferInfo.height          = m_aovSize.height;
  framebufferInfo.layers          = 1;

  if(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_rasterFramebuffer) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create raster framebuffer");
  }
}

void HelloVulkan::createRasterPipeline()
{
  vkDestroyPipeline(m_device, m_rasterPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_rasterPipelineLayout, nullptr);
  destroyRasterFramebuffer();
  vkDestroyRenderPass(m_device, m_rasterRenderPass, nullptr);
  m_rasterPipeline       = VK_NULL_HANDLE;
  m_rasterPipelineLayout = VK_NULL_HANDLE;
  m_rasterRenderPass     = VK_NULL_HANDLE;

  VkPushConstantRange pushConstant{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(PushConstantRaster)};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount         = 1;
  pipelineLayoutCreateInfo.pSetLayouts            = &m_descSetLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

  if(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_rasterPipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create raster pipeline layout");
  }

  std::vector<VkFormat> colorAttachmentFormats{
      m_offscreenColorFormat, m_offscreenObjectIdFormat, m_offscreenInstanceIdFormat, m_offscreenDepthAovFormat};
  m_rasterRenderPass = nvvk::createRenderPass(m_device, colorAttachmentFormats, m_offscreenDepthFormat, 1,
                                              true, true, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);

  nvvk::GraphicsPipelineState pipelineState;
  pipelineState.rasterizationState.cullMode = VK_CULL_MODE_NONE;
  pipelineState.setBlendAttachmentCount(static_cast<uint32_t>(colorAttachmentFormats.size()));
  for(uint32_t attachment = 0; attachment < colorAttachmentFormats.size(); ++attachment)
  {
    pipelineState.setBlendAttachmentState(attachment, nvvk::GraphicsPipelineState::makePipelineColorBlendAttachmentState());
  }
  pipelineState.addBindingDescription(
      nvvk::GraphicsPipelineState::makeVertexInputBinding(0, sizeof(VertexObj), VK_VERTEX_INPUT_RATE_VERTEX));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexObj, pos)));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexObj, nrm)));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexObj, color)));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexObj, texCoord)));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexObj, tangent)));

  nvvk::GraphicsPipelineGenerator pipelineGenerator(m_device, m_rasterPipelineLayout, m_rasterRenderPass, pipelineState);
  pipelineGenerator.addShader(nvh::loadFile("spv/raster.vert.spv", true, defaultSearchPaths, true), VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(nvh::loadFile("spv/raster.frag.spv", true, defaultSearchPaths, true), VK_SHADER_STAGE_FRAGMENT_BIT);
  m_rasterPipeline = pipelineGenerator.createPipeline();
  if(m_rasterPipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error("Failed to create raster graphics pipeline");
  }

  createRasterFramebuffer();
}

void HelloVulkan::rasterize(const VkCommandBuffer& cmdBuf)
{
  if(m_rasterPipeline == VK_NULL_HANDLE || m_rasterPipelineLayout == VK_NULL_HANDLE || m_rasterFramebuffer == VK_NULL_HANDLE
     || m_descSet == VK_NULL_HANDLE)
  {
    return;
  }

  m_debug.beginLabel(cmdBuf, "Rasterize");

  std::array<VkClearValue, 5> clearValues{};
  clearValues[0].color.float32[0] = 0.0f;
  clearValues[0].color.float32[1] = 0.0f;
  clearValues[0].color.float32[2] = 0.0f;
  clearValues[0].color.float32[3] = 1.0f;
  clearValues[1].color.int32[0]   = -1;
  clearValues[2].color.int32[0]   = -1;
  clearValues[3].color.float32[0] = 1.0f;
  clearValues[4].depthStencil     = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass               = m_rasterRenderPass;
  renderPassInfo.framebuffer              = m_rasterFramebuffer;
  renderPassInfo.renderArea.offset        = {0, 0};
  renderPassInfo.renderArea.extent        = m_aovSize;
  renderPassInfo.clearValueCount          = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues             = clearValues.data();

  vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rasterPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rasterPipelineLayout, 0, 1, &m_descSet, 0, nullptr);

  VkViewport viewport{0.0f, 0.0f, static_cast<float>(m_aovSize.width), static_cast<float>(m_aovSize.height), 0.0f, 1.0f};
  VkRect2D   scissor{{0, 0}, m_aovSize};
  vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  for(size_t instanceIndex = 0; instanceIndex < m_instances.size(); ++instanceIndex)
  {
    const ObjInstance& instance = m_instances[instanceIndex];
    if(!instance.visible || instance.objIndex >= m_objModel.size())
    {
      continue;
    }

    const ObjModel& model = m_objModel[instance.objIndex];
    if(model.nbIndices == 0 || model.vertexBuffer.buffer == VK_NULL_HANDLE || model.indexBuffer.buffer == VK_NULL_HANDLE)
    {
      continue;
    }

    VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &model.vertexBuffer.buffer, &vertexOffset);
    vkCmdBindIndexBuffer(cmdBuf, model.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    PushConstantRaster pushConstant{};
    pushConstant.model      = instance.transform;
    pushConstant.objIndex   = instance.objIndex;
    pushConstant.instanceId = instanceIndex < m_instanceIds.size() ? m_instanceIds[instanceIndex] : static_cast<int>(instanceIndex);
    vkCmdPushConstants(cmdBuf, m_rasterPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstantRaster), &pushConstant);
    vkCmdDrawIndexed(cmdBuf, model.nbIndices, 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(cmdBuf);

  m_accumulatedFrames = 1;
  m_frameIndex++;

  m_debug.endLabel(cmdBuf);
}

void HelloVulkan::createDescriptorSetLayout()
{
  auto nbTxt = static_cast<uint32_t>(m_textures.size());

  m_descSetLayoutBind.addBinding(SceneBindings::eFrameUniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_descSetLayoutBind.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_descSetLayoutBind.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTxt,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);

  m_descSetLayout = m_descSetLayoutBind.createLayout(m_device);
  m_descPool      = m_descSetLayoutBind.createPool(m_device, 1);
  m_descSet       = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

void HelloVulkan::updateDescriptorSet()
{
  std::vector<VkWriteDescriptorSet> writes;

  VkDescriptorBufferInfo dbiUnif{m_bFrameUniforms.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eFrameUniforms, &dbiUnif));

  VkDescriptorBufferInfo dbiSceneDesc{m_bObjDesc.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eObjDescs, &dbiSceneDesc));

  std::vector<VkDescriptorImageInfo> diit;
  for(auto& texture : m_textures)
  {
    diit.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_descSetLayoutBind.makeWriteArray(m_descSet, SceneBindings::eTextures, diit.data()));

  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
