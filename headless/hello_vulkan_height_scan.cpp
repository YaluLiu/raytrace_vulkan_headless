#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "hello_vulkan.hpp"
#include "hello_vulkan_barriers.hpp"
#include "nvh/alignment.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/shaders_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;

namespace {
int heightScanAxisSampleCount(float minValue, float maxValue, float stepValue)
{
  const float absStep = std::max(std::abs(stepValue), 1e-4f);
  const float span    = std::abs(maxValue - minValue);
  const int   steps   = static_cast<int>(std::floor(span / absStep + 0.5f));
  return std::max(steps + 1, 1);
}

uint32_t heightScanRayCount(const HeightScanParams& params)
{
  const int xCount = heightScanAxisSampleCount(params.minX, params.maxX, params.stepX);
  const int zCount = heightScanAxisSampleCount(params.minZ, params.maxZ, params.stepZ);
  return static_cast<uint32_t>(std::max(xCount * zCount, 1));
}

bool heightScanVecNearlyEqual(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f)
{
  return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps;
}

bool heightScanParamsNearlyEqual(const HeightScanParams& a, const HeightScanParams& b, float eps = 1e-5f)
{
  return std::fabs(a.minX - b.minX) <= eps && std::fabs(a.maxX - b.maxX) <= eps
         && std::fabs(a.stepX - b.stepX) <= eps && std::fabs(a.minZ - b.minZ) <= eps
         && std::fabs(a.maxZ - b.maxZ) <= eps && std::fabs(a.stepZ - b.stepZ) <= eps
         && std::fabs(a.rayDirectionX - b.rayDirectionX) <= eps
         && std::fabs(a.rayDirectionY - b.rayDirectionY) <= eps
         && std::fabs(a.rayDirectionZ - b.rayDirectionZ) <= eps
         && std::fabs(a.pointRadiusPixels - b.pointRadiusPixels) <= eps
         && std::fabs(a.maxDistance - b.maxDistance) <= eps;
}

}

void HelloVulkan::setHeightScanCamera(const RadarCameraData& camera)
{
  const bool cameraChanged = !m_hasHeightScanCamera || !heightScanVecNearlyEqual(camera.eye, m_heightScanCamera.eye)
                             || !heightScanVecNearlyEqual(camera.center, m_heightScanCamera.center)
                             || !heightScanVecNearlyEqual(camera.up, m_heightScanCamera.up)
                             || std::fabs(camera.fov - m_heightScanCamera.fov) > 1e-5f;

  m_heightScanCamera    = camera;
  m_hasHeightScanCamera = true;

  if(cameraChanged)
  {
    resetAccumulation();
  }
}

void HelloVulkan::setHeightScanParams(const HeightScanParams& params)
{
  if(!heightScanParamsNearlyEqual(params, m_heightScanParams))
  {
    m_heightScanParams = params;
    m_pcRay.heightScan = params;
    resetAccumulation();
  }
}

void HelloVulkan::setHeightScanEnabled(bool enabled)
{
  if(m_enableHeightScan != enabled)
  {
    m_enableHeightScan = enabled;
    resetAccumulation();
  }
}

void HelloVulkan::createHeightScanRtPipeline()
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

  stage.module =
      nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace_height_scan.rgen.spv", true, defaultSearchPaths, true));
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

  m_heightScanRtShaderGroups.clear();

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eRaygen;
  m_heightScanRtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss;
  m_heightScanRtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss2;
  m_heightScanRtShaderGroups.push_back(group);

  group.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  group.generalShader    = VK_SHADER_UNUSED_KHR;
  group.closestHitShader = eClosestHit;
  m_heightScanRtShaderGroups.push_back(group);

  group.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
  group.closestHitShader   = eClosestHit2;
  group.intersectionShader = eIntersection;
  m_heightScanRtShaderGroups.push_back(group);

  VkPushConstantRange pushConstant{
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0,
      sizeof(PushConstantRay)};

  std::vector<VkDescriptorSetLayout> rtDescSetLayouts = {m_lidarRtDescSetLayout, m_descSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;
  pipelineLayoutCreateInfo.setLayoutCount         = static_cast<uint32_t>(rtDescSetLayouts.size());
  pipelineLayoutCreateInfo.pSetLayouts            = rtDescSetLayouts.data();
  vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_heightScanRtPipelineLayout);

  VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
  rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
  rayPipelineInfo.pStages    = stages.data();
  rayPipelineInfo.groupCount = static_cast<uint32_t>(m_heightScanRtShaderGroups.size());
  rayPipelineInfo.pGroups    = m_heightScanRtShaderGroups.data();

  uint32_t desiredRecursionDepth               = 8;
  rayPipelineInfo.maxPipelineRayRecursionDepth = std::min(desiredRecursionDepth, m_rtProperties.maxRayRecursionDepth);
  rayPipelineInfo.layout                       = m_heightScanRtPipelineLayout;
  vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_heightScanRtPipeline);

  for(auto& shaderStage : stages)
  {
    vkDestroyShaderModule(m_device, shaderStage.module, nullptr);
  }
}

void HelloVulkan::createHeightScanRtShaderBindingTable()
{
  uint32_t missCount{2};
  uint32_t hitCount{2};
  uint32_t handleCount = 1 + missCount + hitCount;
  uint32_t handleSize  = m_rtProperties.shaderGroupHandleSize;

  uint32_t handleSizeAligned = nvh::align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

  m_heightScanRtRgenRegion.stride = nvh::align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_heightScanRtRgenRegion.size   = m_heightScanRtRgenRegion.stride;
  m_heightScanRtMissRegion.stride = handleSizeAligned;
  m_heightScanRtMissRegion.size   = nvh::align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_heightScanRtHitRegion.stride  = handleSizeAligned;
  m_heightScanRtHitRegion.size    = nvh::align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

  uint32_t             dataSize = handleCount * handleSize;
  std::vector<uint8_t> handles(dataSize);
  auto result =
      vkGetRayTracingShaderGroupHandlesKHR(m_device, m_heightScanRtPipeline, 0, handleCount, dataSize, handles.data());
  assert(result == VK_SUCCESS);

  VkDeviceSize sbtSize = m_heightScanRtRgenRegion.size + m_heightScanRtMissRegion.size + m_heightScanRtHitRegion.size
                         + m_heightScanRtCallRegion.size;
  m_heightScanRtSBTBuffer = m_alloc.createBuffer(sbtSize,
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                     | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_debug.setObjectName(m_heightScanRtSBTBuffer.buffer, std::string("Height Scan RT SBT"));

  VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_heightScanRtSBTBuffer.buffer};
  VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(m_device, &info);
  m_heightScanRtRgenRegion.deviceAddress = sbtAddress;
  m_heightScanRtMissRegion.deviceAddress = sbtAddress + m_heightScanRtRgenRegion.size;
  m_heightScanRtHitRegion.deviceAddress  = sbtAddress + m_heightScanRtRgenRegion.size + m_heightScanRtMissRegion.size;

  auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

  auto*    pSBTBuffer = reinterpret_cast<uint8_t*>(m_alloc.map(m_heightScanRtSBTBuffer));
  uint8_t* pData{nullptr};
  uint32_t handleIdx{0};

  pData = pSBTBuffer;
  memcpy(pData, getHandle(handleIdx++), handleSize);

  pData = pSBTBuffer + m_heightScanRtRgenRegion.size;
  for(uint32_t c = 0; c < missCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_heightScanRtMissRegion.stride;
  }

  pData = pSBTBuffer + m_heightScanRtRgenRegion.size + m_heightScanRtMissRegion.size;
  for(uint32_t c = 0; c < hitCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_heightScanRtHitRegion.stride;
  }

  m_alloc.unmap(m_heightScanRtSBTBuffer);
  m_alloc.finalizeAndReleaseStaging();
}

void HelloVulkan::renderHeightScanPointCloud(const VkCommandBuffer& cmdBuf)
{
  if(cmdBuf == VK_NULL_HANDLE || m_heightScanRtPipeline == VK_NULL_HANDLE || m_lidarRtDescSet == VK_NULL_HANDLE
     || !m_enableHeightScan)
  {
    return;
  }

  m_debug.beginLabel(cmdBuf, "Height scan point cloud");

  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
                                | VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  std::vector<VkDescriptorSet> descSets{m_lidarRtDescSet, m_descSet};
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_heightScanRtPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_heightScanRtPipelineLayout, 0,
                          static_cast<uint32_t>(descSets.size()), descSets.data(), 0, nullptr);

  PushConstantRay heightScanPassPc = m_pcRay;
  heightScanPassPc.lidarPassMode   = eRaygenPassLidarPointCloud;
  heightScanPassPc.numLights       = 0;
  heightScanPassPc.maxDepth        = 1;
  heightScanPassPc.samplesPerFrame = 1;
  heightScanPassPc.jitterX         = 0.0f;
  heightScanPassPc.jitterY         = 0.0f;
  heightScanPassPc.heightScan      = m_heightScanParams;

  const VkShaderStageFlags pushStages =
      VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
  vkCmdPushConstants(cmdBuf, m_heightScanRtPipelineLayout, pushStages, 0, sizeof(PushConstantRay), &heightScanPassPc);

  vkCmdTraceRaysKHR(cmdBuf, &m_heightScanRtRgenRegion, &m_heightScanRtMissRegion, &m_heightScanRtHitRegion,
                    &m_heightScanRtCallRegion, heightScanRayCount(heightScanPassPc.heightScan), 1, 1);

  insertGeneralImageBarrier(cmdBuf, m_offscreenLidarPointCloudDepthKey.image, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  m_debug.endLabel(cmdBuf);
}
