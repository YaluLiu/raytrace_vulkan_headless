#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include "nvvk/stagingmemorymanager_vk.hpp"


extern std::vector<std::string> defaultSearchPaths;

namespace {
struct ResolvedCamera {
  glm::vec3 position{0.0f};
  glm::vec3 target{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float     vfovDeg{45.0f};
  float     clipStart{0.1f};
  float     clipEnd{1000.0f};
};

std::pair<float, float> sanitizeClipRange(float clipStart, float clipEnd)
{
  const float safeStart = std::max(clipStart, 0.001f);
  const float safeEnd   = std::max(clipEnd, safeStart + 0.001f);
  return {safeStart, safeEnd};
}

VkDeviceSize alignTo(VkDeviceSize value, VkDeviceSize alignment)
{
  if(alignment == 0)
  {
    return value;
  }
  return ((value + alignment - 1) / alignment) * alignment;
}

ResolvedCamera resolveCamera(const HeadlessCameraData& camera)
{
  ResolvedCamera resolved;
  resolved.position = camera.position;

  glm::vec3 forward = camera.forward;
  if(glm::length(forward) < 1e-6f)
  {
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  }

  resolved.up = camera.up;
  if(glm::length(glm::cross(forward, resolved.up)) < 1e-6f)
  {
    resolved.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  resolved.target  = resolved.position + forward;
  resolved.vfovDeg = std::clamp(camera.vfov_deg, 1.0f, 179.0f);
  const auto [clipStart, clipEnd] = sanitizeClipRange(camera.clipStart, camera.clipEnd);
  resolved.clipStart             = clipStart;
  resolved.clipEnd               = clipEnd;
  return resolved;
}

FrameUniforms makeFrameUniforms(const glm::mat4& view,
                                float           vfovDeg,
                                float           clipStart,
                                float           clipEnd,
                                VkExtent2D      renderSize,
                                size_t          lightCount)
{
  FrameUniforms frameUBO = {};
  const float   aspectRatio = renderSize.width / static_cast<float>(renderSize.height);
  glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(vfovDeg), aspectRatio, clipStart, clipEnd);

  frameUBO.camera.viewProj    = proj * view;
  frameUBO.camera.view        = view;
  frameUBO.camera.viewInverse = glm::inverse(view);
  frameUBO.camera.projInverse = glm::inverse(proj);
  frameUBO.lightCount         = static_cast<uint32_t>(std::min<size_t>(lightCount, MAX_SCENE_LIGHTS));
  return frameUBO;
}

void recordFrameUniformUpdate(const VkCommandBuffer& cmdBuf,
                              VkBuffer deviceUBO,
                              VkDeviceSize offset,
                              const FrameUniforms& frameUBO)
{
  auto uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  beforeBarrier.buffer        = deviceUBO;
  beforeBarrier.offset        = offset;
  beforeBarrier.size          = sizeof(frameUBO);
  vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &beforeBarrier, 0, nullptr);

  vkCmdUpdateBuffer(cmdBuf, deviceUBO, offset, sizeof(FrameUniforms), &frameUBO);

  VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  afterBarrier.buffer        = deviceUBO;
  afterBarrier.offset        = offset;
  afterBarrier.size          = sizeof(frameUBO);
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &afterBarrier, 0, nullptr);
}
}

void HelloVulkan::setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily)
{
  AppOffline::setup(instance, device, physicalDevice, queueFamily);
  m_alloc.init(device, physicalDevice);
  m_alloc.getStaging()->setFreeUnusedOnRelease(false);
  m_debug.setup(m_device);
  VkPhysicalDeviceProperties deviceProperties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  m_frameUniformStride = alignTo(sizeof(FrameUniforms), deviceProperties.limits.minUniformBufferOffsetAlignment);
  m_mainRasterPipeline.setup(m_device, physicalDevice, queueFamily, m_alloc, m_debug);
  m_tileRasterPipeline.setup(m_device, physicalDevice, queueFamily, m_alloc, m_debug);
  m_tileAtlas.setup(m_device, queueFamily, m_alloc, m_debug);
}

std::optional<HeadlessAovTexture> HelloVulkan::GetAovTexture(HeadlessAov aov) const
{
  return m_mainRasterPipeline.getAovTexture(aov);
}

void HelloVulkan::setMainCameraClipRange(float clipStart, float clipEnd)
{
  const auto [safeStart, safeEnd] = sanitizeClipRange(clipStart, clipEnd);
  if(std::fabs(m_mainCameraClipStart - safeStart) > 1e-6f || std::fabs(m_mainCameraClipEnd - safeEnd) > 1e-6f)
  {
    m_mainCameraClipStart = safeStart;
    m_mainCameraClipEnd   = safeEnd;
  }
}

void HelloVulkan::setCameras(std::vector<HeadlessCameraData> cameras)
{
  m_cameras = std::move(cameras);
  ensureFrameUniformCapacity(getRequiredFrameUniformSlots());
}

void HelloVulkan::setMainCamera(const HeadlessCameraData& camera)
{
  const ResolvedCamera resolved = resolveCamera(camera);
  m_mainCameraClipStart         = resolved.clipStart;
  m_mainCameraClipEnd           = resolved.clipEnd;
  CameraManip.setCamera({resolved.position, resolved.target, resolved.up, resolved.vfovDeg});
}

void HelloVulkan::setTileConfig(HeadlessTileConfig config)
{
  config.sanitize();
  if(config != m_tileConfig)
  {
    m_tileConfig = config;
    if(!m_tileConfig.enabled)
    {
      m_tileAtlasDirty = false;
    }
    ensureFrameUniformCapacity(getRequiredFrameUniformSlots());
  }
}

void HelloVulkan::updateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
  updateUniformBufferForExtent(cmdBuf, m_size);
}

void HelloVulkan::updateUniformBufferForExtent(const VkCommandBuffer& cmdBuf, VkExtent2D renderSize)
{
  if(renderSize.width == 0 || renderSize.height == 0)
  {
    return;
  }

  const glm::mat4 view = CameraManip.getMatrix();
  const FrameUniforms frameUBO =
      makeFrameUniforms(view, CameraManip.getFov(), m_mainCameraClipStart, m_mainCameraClipEnd, renderSize,
                        m_lights.size());
  recordFrameUniformUpdate(cmdBuf, m_bFrameUniforms.buffer, 0, frameUBO);
}

void HelloVulkan::updateUniformBufferForCamera(const VkCommandBuffer& cmdBuf,
                                               const HeadlessCameraData& camera,
                                               VkExtent2D renderSize,
                                               uint32_t slotIndex)
{
  if(renderSize.width == 0 || renderSize.height == 0)
  {
    return;
  }

  const ResolvedCamera resolved = resolveCamera(camera);
  const glm::mat4      view = glm::lookAtRH(resolved.position, resolved.target, resolved.up);
  const FrameUniforms  frameUBO =
      makeFrameUniforms(view, resolved.vfovDeg, resolved.clipStart, resolved.clipEnd, renderSize, m_lights.size());
  recordFrameUniformUpdate(cmdBuf, m_bFrameUniforms.buffer, getFrameUniformOffset(slotIndex), frameUBO);
}

void HelloVulkan::createUniformBuffer()
{
  ensureFrameUniformCapacity(getRequiredFrameUniformSlots());
}

uint32_t HelloVulkan::getRequiredFrameUniformSlots() const
{
  uint32_t slotCount = 1;
  if(m_tileConfig.enabled && !m_cameras.empty())
  {
    HeadlessTileConfig config = m_tileConfig;
    config.sanitize();
    slotCount += static_cast<uint32_t>(std::min<size_t>(m_cameras.size(), config.capacity()));
  }
  return slotCount;
}

uint32_t HelloVulkan::getFrameUniformOffset(uint32_t slotIndex) const
{
  return static_cast<uint32_t>(m_frameUniformStride * slotIndex);
}

void HelloVulkan::ensureFrameUniformCapacity(uint32_t slotCount)
{
  slotCount = std::max<uint32_t>(slotCount, 1);
  if(m_device == VK_NULL_HANDLE || (m_bFrameUniforms.buffer != VK_NULL_HANDLE && slotCount <= m_frameUniformSlotCount))
  {
    return;
  }

  if(m_frameUniformStride == 0)
  {
    m_frameUniformStride = sizeof(FrameUniforms);
  }

  m_alloc.destroy(m_bFrameUniforms);
  m_bFrameUniforms = m_alloc.createBuffer(m_frameUniformStride * slotCount,
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_frameUniformSlotCount = slotCount;
  m_debug.setObjectName(m_bFrameUniforms.buffer, "FrameUniforms");
  updateFrameUniformDescriptor();
}

void HelloVulkan::updateFrameUniformDescriptor()
{
  if(m_descSet == VK_NULL_HANDLE || m_descSetLayout == VK_NULL_HANDLE || m_bFrameUniforms.buffer == VK_NULL_HANDLE)
  {
    return;
  }

  VkDescriptorBufferInfo dbiUnif{m_bFrameUniforms.buffer, 0, sizeof(FrameUniforms)};
  VkWriteDescriptorSet write = m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eFrameUniforms, &dbiUnif);
  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void HelloVulkan::createObjDescriptionBuffer()
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);

  auto cmdBuf = cmdGen.createCommandBuffer();
  const std::vector<ObjDesc> dummyObjDesc(1);
  const std::vector<ObjDesc>& objDesc = m_objDesc.empty() ? dummyObjDesc : m_objDesc;
  m_bObjDesc  = m_alloc.createBuffer(cmdBuf, objDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  cmdGen.submitAndWait(cmdBuf);
  m_alloc.finalizeAndReleaseStaging();
  m_debug.setObjectName(m_bObjDesc.buffer, "ObjDescs");
}

void HelloVulkan::destroyResources()
{
  m_tileAtlas.destroy();
  m_tileRasterPipeline.destroy();
  m_mainRasterPipeline.destroy();

  vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descSetLayout, nullptr);

  m_alloc.destroy(m_bFrameUniforms);
  m_bFrameUniforms        = {};
  m_frameUniformSlotCount = 0;
  m_alloc.destroy(m_bObjDesc);
  m_alloc.destroy(m_bLights);

  for(auto& m : m_objModel)
  {
    m_alloc.destroy(m.vertexBuffer);
    m_alloc.destroy(m.indexBuffer);
    m_alloc.destroy(m.matColorBuffer);
    m_alloc.destroy(m.matIndexBuffer);
  }

  for(auto& t : m_textures)
  {
    m_alloc.destroy(t);
  }

  if(m_compPipeline != VK_NULL_HANDLE)
    vkDestroyPipeline(m_device, m_compPipeline, nullptr);
  if(m_compPipelineLayout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(m_device, m_compPipelineLayout, nullptr);
  if(m_compDescPool != VK_NULL_HANDLE)
    vkDestroyDescriptorPool(m_device, m_compDescPool, nullptr);
  if(m_compDescSetLayout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(m_device, m_compDescSetLayout, nullptr);

  m_alloc.deinit();
}

void HelloVulkan::onResize(int w, int h)
{
  if(w <= 0 || h <= 0)
  {
    return;
  }

  if(w == (int)m_size.width && h == (int)m_size.height)
  {
    return;
  }

  m_size.width  = w;
  m_size.height = h;
  createOffscreenRender();
}

void HelloVulkan::createOffscreenRender()
{
  m_mainRasterPipeline.createOffscreenRender(m_size);
}
