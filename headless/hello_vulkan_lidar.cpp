#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "hello_vulkan.hpp"
#include "nvh/alignment.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/shaders_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;

namespace {
int lidarAxisSampleCount(float minDeg, float maxDeg, float stepDeg)
{
  const float absStep = std::max(std::abs(stepDeg), 1e-4f);
  const float span    = std::abs(maxDeg - minDeg);
  const int   steps   = static_cast<int>(std::floor(span / absStep + 0.5f));
  return std::max(steps + 1, 1);
}

uint32_t lidarRayCount(const LidarParams& params)
{
  const int azCount = lidarAxisSampleCount(params.azimuthMinDeg, params.azimuthMaxDeg, params.azimuthStepDeg);
  const int elCount = lidarAxisSampleCount(params.verticalMinDeg, params.verticalMaxDeg, params.verticalStepDeg);
  return static_cast<uint32_t>(std::max(azCount * elCount, 1));
}

void insertGeneralImageBarrier(VkCommandBuffer cmdBuf, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                               VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.srcAccessMask       = srcAccessMask;
  barrier.dstAccessMask       = dstAccessMask;
  barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
  barrier.image               = image;
  barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(cmdBuf, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
}  // namespace

void HelloVulkan::createLidarRtDescriptorSet()
{
  m_lidarRtDescSetLayoutBind.addBinding(RtxBindings::eTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                                        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  m_lidarRtDescSetLayoutBind.addBinding(RtxBindings::eLidarDepthKeyImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                        VK_SHADER_STAGE_RAYGEN_BIT_KHR);

  m_lidarRtDescPool      = m_lidarRtDescSetLayoutBind.createPool(m_device);
  m_lidarRtDescSetLayout = m_lidarRtDescSetLayoutBind.createLayout(m_device);

  VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocateInfo.descriptorPool     = m_lidarRtDescPool;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts        = &m_lidarRtDescSetLayout;
  vkAllocateDescriptorSets(m_device, &allocateInfo, &m_lidarRtDescSet);

  VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
  VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
  descASInfo.accelerationStructureCount = 1;
  descASInfo.pAccelerationStructures    = &tlas;
  VkDescriptorImageInfo lidarDepthKeyInfo{{}, m_offscreenLidarPointCloudDepthKey.descriptor.imageView,
                                          VK_IMAGE_LAYOUT_GENERAL};

  std::array<VkWriteDescriptorSet, 2> writes{
      m_lidarRtDescSetLayoutBind.makeWrite(m_lidarRtDescSet, RtxBindings::eTlas, &descASInfo),
      m_lidarRtDescSetLayoutBind.makeWrite(m_lidarRtDescSet, RtxBindings::eLidarDepthKeyImage, &lidarDepthKeyInfo),
  };
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void HelloVulkan::updateLidarRtDescriptorSet()
{
  VkDescriptorImageInfo lidarDepthKeyInfo{{}, m_offscreenLidarPointCloudDepthKey.descriptor.imageView,
                                          VK_IMAGE_LAYOUT_GENERAL};
  VkWriteDescriptorSet  write =
      m_lidarRtDescSetLayoutBind.makeWrite(m_lidarRtDescSet, RtxBindings::eLidarDepthKeyImage, &lidarDepthKeyInfo);
  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void HelloVulkan::createLidarRtPipeline()
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

  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace_lidar.rgen.spv", true, defaultSearchPaths, true));
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

  m_lidarRtShaderGroups.clear();

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eRaygen;
  m_lidarRtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss;
  m_lidarRtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss2;
  m_lidarRtShaderGroups.push_back(group);

  group.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  group.generalShader    = VK_SHADER_UNUSED_KHR;
  group.closestHitShader = eClosestHit;
  m_lidarRtShaderGroups.push_back(group);

  group.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
  group.closestHitShader   = eClosestHit2;
  group.intersectionShader = eIntersection;
  m_lidarRtShaderGroups.push_back(group);

  VkPushConstantRange pushConstant{
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0,
      sizeof(PushConstantRay)};

  std::vector<VkDescriptorSetLayout> rtDescSetLayouts = {m_lidarRtDescSetLayout, m_descSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;
  pipelineLayoutCreateInfo.setLayoutCount         = static_cast<uint32_t>(rtDescSetLayouts.size());
  pipelineLayoutCreateInfo.pSetLayouts            = rtDescSetLayouts.data();
  vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_lidarRtPipelineLayout);

  VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
  rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
  rayPipelineInfo.pStages    = stages.data();
  rayPipelineInfo.groupCount = static_cast<uint32_t>(m_lidarRtShaderGroups.size());
  rayPipelineInfo.pGroups    = m_lidarRtShaderGroups.data();

  uint32_t desiredRecursionDepth               = 8;
  rayPipelineInfo.maxPipelineRayRecursionDepth = std::min(desiredRecursionDepth, m_rtProperties.maxRayRecursionDepth);
  rayPipelineInfo.layout                       = m_lidarRtPipelineLayout;
  vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_lidarRtPipeline);

  for(auto& shaderStage : stages)
  {
    vkDestroyShaderModule(m_device, shaderStage.module, nullptr);
  }
}

void HelloVulkan::createLidarRtShaderBindingTable()
{
  uint32_t missCount{2};
  uint32_t hitCount{2};
  uint32_t handleCount = 1 + missCount + hitCount;
  uint32_t handleSize  = m_rtProperties.shaderGroupHandleSize;

  uint32_t handleSizeAligned = nvh::align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

  m_lidarRtRgenRegion.stride = nvh::align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_lidarRtRgenRegion.size   = m_lidarRtRgenRegion.stride;
  m_lidarRtMissRegion.stride = handleSizeAligned;
  m_lidarRtMissRegion.size   = nvh::align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_lidarRtHitRegion.stride  = handleSizeAligned;
  m_lidarRtHitRegion.size    = nvh::align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

  uint32_t             dataSize = handleCount * handleSize;
  std::vector<uint8_t> handles(dataSize);
  auto result =
      vkGetRayTracingShaderGroupHandlesKHR(m_device, m_lidarRtPipeline, 0, handleCount, dataSize, handles.data());
  assert(result == VK_SUCCESS);

  VkDeviceSize sbtSize =
      m_lidarRtRgenRegion.size + m_lidarRtMissRegion.size + m_lidarRtHitRegion.size + m_lidarRtCallRegion.size;
  m_lidarRtSBTBuffer = m_alloc.createBuffer(sbtSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_debug.setObjectName(m_lidarRtSBTBuffer.buffer, std::string("Lidar RT SBT"));

  VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_lidarRtSBTBuffer.buffer};
  VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(m_device, &info);
  m_lidarRtRgenRegion.deviceAddress    = sbtAddress;
  m_lidarRtMissRegion.deviceAddress    = sbtAddress + m_lidarRtRgenRegion.size;
  m_lidarRtHitRegion.deviceAddress     = sbtAddress + m_lidarRtRgenRegion.size + m_lidarRtMissRegion.size;

  auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

  auto*    pSBTBuffer = reinterpret_cast<uint8_t*>(m_alloc.map(m_lidarRtSBTBuffer));
  uint8_t* pData{nullptr};
  uint32_t handleIdx{0};

  pData = pSBTBuffer;
  memcpy(pData, getHandle(handleIdx++), handleSize);

  pData = pSBTBuffer + m_lidarRtRgenRegion.size;
  for(uint32_t c = 0; c < missCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_lidarRtMissRegion.stride;
  }

  pData = pSBTBuffer + m_lidarRtRgenRegion.size + m_lidarRtMissRegion.size;
  for(uint32_t c = 0; c < hitCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_lidarRtHitRegion.stride;
  }

  m_alloc.unmap(m_lidarRtSBTBuffer);
  m_alloc.finalizeAndReleaseStaging();
}

void HelloVulkan::renderLidarPointCloud(const VkCommandBuffer& cmdBuf)
{
  if(cmdBuf == VK_NULL_HANDLE || m_lidarRtPipeline == VK_NULL_HANDLE || m_lidarRtDescSet == VK_NULL_HANDLE)
  {
    return;
  }

  m_debug.beginLabel(cmdBuf, "Lidar point cloud");

  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloud.image, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT);
  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkClearColorValue       clearValue{};
  VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdClearColorImage(cmdBuf, m_offscreenLidarPointCloud.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
  VkClearColorValue clearDepthKeyValue{};
  clearDepthKeyValue.uint32[0] = 0xffffffffu;
  vkCmdClearColorImage(cmdBuf, m_offscreenLidarPointCloudDepthKey.image, VK_IMAGE_LAYOUT_GENERAL, &clearDepthKeyValue, 1,
                       &clearRange);

  if(!m_enableLidar)
  {
    insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloud.image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image, VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    m_debug.endLabel(cmdBuf);
    return;
  }

  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloud.image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  std::vector<VkDescriptorSet> descSets{m_lidarRtDescSet, m_descSet};
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_lidarRtPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_lidarRtPipelineLayout, 0,
                          static_cast<uint32_t>(descSets.size()), descSets.data(), 0, nullptr);

  PushConstantRay lidarPassPc = m_pcRay;
  lidarPassPc.lidarPassMode   = eRaygenPassLidarPointCloud;
  lidarPassPc.numLights       = 0;
  lidarPassPc.maxDepth        = 1;
  lidarPassPc.samplesPerFrame = 1;
  lidarPassPc.jitterX         = 0.0f;
  lidarPassPc.jitterY         = 0.0f;

  const VkShaderStageFlags pushStages =
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
  vkCmdPushConstants(cmdBuf, m_lidarRtPipelineLayout, pushStages, 0, sizeof(PushConstantRay), &lidarPassPc);

  vkCmdTraceRaysKHR(cmdBuf, &m_lidarRtRgenRegion, &m_lidarRtMissRegion, &m_lidarRtHitRegion, &m_lidarRtCallRegion,
                    lidarRayCount(lidarPassPc.lidar), 1, 1);

  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  m_debug.endLabel(cmdBuf);
}

void HelloVulkan::createLidarCompositePipeline()
{
  m_lidarCompositeDescSetLayoutBind.addBinding(LidarCompositeBindings::eCompositeDenoisedImage,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  m_lidarCompositeDescSetLayoutBind.addBinding(LidarCompositeBindings::eCompositeLidarPointCloudImage,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  m_lidarCompositeDescSetLayoutBind.addBinding(LidarCompositeBindings::eCompositeDepthImage,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  m_lidarCompositeDescSetLayoutBind.addBinding(LidarCompositeBindings::eCompositeLidarDepthKeyImage,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

  m_lidarCompositeDescSetLayout = m_lidarCompositeDescSetLayoutBind.createLayout(m_device);
  m_lidarCompositeDescPool      = m_lidarCompositeDescSetLayoutBind.createPool(m_device, 1);
  m_lidarCompositeDescSet       = nvvk::allocateDescriptorSet(m_device, m_lidarCompositeDescPool, m_lidarCompositeDescSetLayout);
  updateLidarCompositeDescriptorSet();

  VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  createInfo.setLayoutCount = 1;
  createInfo.pSetLayouts    = &m_lidarCompositeDescSetLayout;
  vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_lidarCompositePipelineLayout);

  VkComputePipelineCreateInfo computePipelineCreateInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  computePipelineCreateInfo.layout = m_lidarCompositePipelineLayout;
  computePipelineCreateInfo.stage =
      nvvk::createShaderStageInfo(m_device, nvh::loadFile("spv/lidar_composite.comp.spv", true, defaultSearchPaths, true),
                                  VK_SHADER_STAGE_COMPUTE_BIT);

  vkCreateComputePipelines(m_device, {}, 1, &computePipelineCreateInfo, nullptr, &m_lidarCompositePipeline);
  vkDestroyShaderModule(m_device, computePipelineCreateInfo.stage.module, nullptr);
}

void HelloVulkan::updateLidarCompositeDescriptorSet()
{
  VkDescriptorImageInfo denoisedInfo{{}, m_offscreenDenoised.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo lidarPointCloudInfo{{}, m_offscreenLidarPointCloud.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo depthInfo{{}, m_offscreenDepthAov.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo lidarDepthKeyInfo{{}, m_offscreenLidarPointCloudDepthKey.descriptor.imageView,
                                          VK_IMAGE_LAYOUT_GENERAL};

  std::array<VkWriteDescriptorSet, 4> writes{
      m_lidarCompositeDescSetLayoutBind.makeWrite(m_lidarCompositeDescSet, LidarCompositeBindings::eCompositeDenoisedImage,
                                                  &denoisedInfo),
      m_lidarCompositeDescSetLayoutBind.makeWrite(m_lidarCompositeDescSet,
                                                  LidarCompositeBindings::eCompositeLidarPointCloudImage,
                                                  &lidarPointCloudInfo),
      m_lidarCompositeDescSetLayoutBind.makeWrite(m_lidarCompositeDescSet, LidarCompositeBindings::eCompositeDepthImage,
                                                  &depthInfo),
      m_lidarCompositeDescSetLayoutBind.makeWrite(m_lidarCompositeDescSet,
                                                  LidarCompositeBindings::eCompositeLidarDepthKeyImage,
                                                  &lidarDepthKeyInfo),
  };
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void HelloVulkan::compositeLidar(const VkCommandBuffer& cmdBuf)
{
  if(cmdBuf == VK_NULL_HANDLE || !m_enableLidar || m_lidarCompositePipeline == VK_NULL_HANDLE
     || m_lidarCompositeDescSet == VK_NULL_HANDLE || m_size.width == 0 || m_size.height == 0)
  {
    return;
  }

  m_debug.beginLabel(cmdBuf, "Lidar composite");

  insertGeneralImageBarrier(cmdBuf, m_offscreenDenoised.image,
                            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloud.image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image,
                            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  insertGeneralImageBarrier(cmdBuf, m_offscreenDepthAov.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_lidarCompositePipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_lidarCompositePipelineLayout, 0, 1,
                          &m_lidarCompositeDescSet, 0, nullptr);

  constexpr uint32_t kLocalSize = 16;
  const uint32_t     groupX     = (m_size.width + kLocalSize - 1) / kLocalSize;
  const uint32_t     groupY     = (m_size.height + kLocalSize - 1) / kLocalSize;
  vkCmdDispatch(cmdBuf, groupX, groupY, 1);

  insertGeneralImageBarrier(cmdBuf, m_offscreenDenoised.image, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloud.image, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

  m_debug.endLabel(cmdBuf);
}
