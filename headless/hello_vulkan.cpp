#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

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


extern std::vector<std::string> defaultSearchPaths;

namespace {
bool matrixNearlyEqual(const glm::mat4& a, const glm::mat4& b, float eps = 1e-5f)
{
  for(int c = 0; c < 4; ++c)
  {
    for(int r = 0; r < 4; ++r)
    {
      if(std::fabs(a[c][r] - b[c][r]) > eps)
      {
        return false;
      }
    }
  }
  return true;
}

bool requiresDedicatedImageAllocation(VkDevice device, VkImage image)
{
  if(device == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
  {
    return false;
  }

  VkMemoryDedicatedRequirements  dedicatedRequirements{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
  VkMemoryRequirements2          memoryRequirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  VkImageMemoryRequirementsInfo2 imageRequirements{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
  imageRequirements.image = image;
  memoryRequirements.pNext = &dedicatedRequirements;

  vkGetImageMemoryRequirements2(device, &imageRequirements, &memoryRequirements);
  return dedicatedRequirements.requiresDedicatedAllocation == VK_TRUE;
}
}

void HelloVulkan::setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily)
{
  AppOffline::setup(instance, device, physicalDevice, queueFamily);
  m_alloc.init(device, physicalDevice);
  m_debug.setup(m_device);
  m_offscreenDepthFormat = nvvk::findDepthFormat(physicalDevice);
  m_sharedAlloc.init(device, physicalDevice);
  if(const char* sppEnv = std::getenv("RASTER_SPP"))
  {
    char* end = nullptr;
    long  spp = std::strtol(sppEnv, &end, 10);
    if(end != sppEnv && end != nullptr && *end == '\0')
    {
      m_samplesPerFrame = std::clamp(static_cast<int>(spp), 1, 64);
    }
  }
  m_frameIndex = 0;
  resetFrameHistory();
  m_hasLastCamera = false;
}

void HelloVulkan::resetAccumulation()
{
  m_accumulatedFrames = 0;
}

void HelloVulkan::resetFrameHistory()
{
  resetAccumulation();
}

std::optional<HeadlessAovTexture> HelloVulkan::GetAovTexture(HeadlessAov aov) const
{
  const nvvk::Texture* texture = nullptr;
  VkFormat             format  = VK_FORMAT_UNDEFINED;
  VkExtent2D           extent  = {0, 0};

  switch(aov)
  {
    case HeadlessAov::Color:
      texture = &m_offscreenColor;
      format  = m_offscreenColorFormat;
      extent  = m_size;
      break;
    case HeadlessAov::PrimId:
      texture = &m_offscreenObjectId;
      format  = m_offscreenObjectIdFormat;
      extent  = m_aovSize;
      break;
    case HeadlessAov::InstanceId:
      texture = &m_offscreenInstanceId;
      format  = m_offscreenInstanceIdFormat;
      extent  = m_aovSize;
      break;
    case HeadlessAov::Depth:
      texture = &m_offscreenDepthAov;
      format  = m_offscreenDepthAovFormat;
      extent  = m_aovSize;
      break;
  }

  if(texture == nullptr || texture->image == VK_NULL_HANDLE || texture->descriptor.imageView == VK_NULL_HANDLE
     || texture->memHandle == nullptr)
  {
    return std::nullopt;
  }

  auto* memAlloc = m_sharedAlloc.getMemoryAllocator();
  if(memAlloc == nullptr)
  {
    return std::nullopt;
  }

  const auto memoryInfo = memAlloc->getMemoryInfo(texture->memHandle);
  if(memoryInfo.memory == VK_NULL_HANDLE)
  {
    return std::nullopt;
  }

  HeadlessAovTexture result;
  result.device       = m_device;
  result.image        = texture->image;
  result.imageView    = texture->descriptor.imageView;
  result.memory       = memoryInfo.memory;
  result.memoryOffset = memoryInfo.offset;
  result.memorySize   = memoryInfo.size;
  result.format       = format;
  result.extent       = extent;
  result.layout       = texture->descriptor.imageLayout;
  result.dedicatedMemory = requiresDedicatedImageAllocation(m_device, texture->image);
  return result;
}

void HelloVulkan::setMainCameraClipRange(float clipStart, float clipEnd)
{
  const float safeStart = std::max(clipStart, 0.001f);
  const float safeEnd   = std::max(clipEnd, safeStart + 0.001f);
  if(std::fabs(m_mainCameraClipStart - safeStart) > 1e-6f || std::fabs(m_mainCameraClipEnd - safeEnd) > 1e-6f)
  {
    m_mainCameraClipStart = safeStart;
    m_mainCameraClipEnd   = safeEnd;
    resetFrameHistory();
    m_hasLastCamera = false;
  }
}

VkExtent2D HelloVulkan::computeRenderSize()
{
  return m_size;
}

void HelloVulkan::refreshOffscreenRenderTargetsIfNeeded()
{
  if(m_device == VK_NULL_HANDLE || m_size.width == 0 || m_size.height == 0)
  {
    return;
  }

  const VkExtent2D desiredRenderSize = computeRenderSize();
  if(desiredRenderSize.width == m_renderSize.width && desiredRenderSize.height == m_renderSize.height)
  {
    return;
  }

  createOffscreenRender();
  refreshOffscreenRenderTargetDescriptors();
}

void HelloVulkan::refreshOffscreenRenderTargetDescriptors()
{
}

void HelloVulkan::updateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
  const float    aspectRatio = m_size.width / static_cast<float>(m_size.height);
  FrameUniforms  frameUBO    = {};
  const auto&    view        = CameraManip.getMatrix();
  glm::mat4      proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, m_mainCameraClipStart,
                                         m_mainCameraClipEnd);

  const bool cameraChanged = !m_hasLastCamera || !matrixNearlyEqual(view, m_lastView) || !matrixNearlyEqual(proj, m_lastProj);
  const glm::mat4 prevViewProj = m_hasLastCamera ? (m_lastProj * m_lastView) : (proj * view);
  if(cameraChanged)
  {
    resetAccumulation();
  }

  frameUBO.camera.viewProj     = proj * view;
  frameUBO.camera.view         = view;
  frameUBO.camera.viewInverse  = glm::inverse(view);
  frameUBO.camera.projInverse  = glm::inverse(proj);
  frameUBO.camera.prevViewProj = prevViewProj;
  frameUBO.lightCount = static_cast<uint32_t>(std::min<size_t>(m_lights.size(), MAX_SCENE_LIGHTS));

  m_lastView      = view;
  m_lastProj      = proj;
  m_hasLastCamera = true;

  VkBuffer deviceUBO      = m_bFrameUniforms.buffer;
  auto     uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  beforeBarrier.buffer        = deviceUBO;
  beforeBarrier.offset        = 0;
  beforeBarrier.size          = sizeof(frameUBO);
  vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &beforeBarrier, 0, nullptr);

  vkCmdUpdateBuffer(cmdBuf, m_bFrameUniforms.buffer, 0, sizeof(FrameUniforms), &frameUBO);

  VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  afterBarrier.buffer        = deviceUBO;
  afterBarrier.offset        = 0;
  afterBarrier.size          = sizeof(frameUBO);
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &afterBarrier, 0, nullptr);
}

void HelloVulkan::createUniformBuffer()
{
  m_bFrameUniforms = m_alloc.createBuffer(sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_debug.setObjectName(m_bFrameUniforms.buffer, "FrameUniforms");
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
  vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descSetLayout, nullptr);

  m_alloc.destroy(m_bFrameUniforms);
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

  destroyRasterFramebuffer();
  vkDestroyPipeline(m_device, m_rasterPipeline, nullptr);
  vkDestroyPipeline(m_device, m_domeBackgroundPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_rasterPipelineLayout, nullptr);
  vkDestroyRenderPass(m_device, m_rasterRenderPass, nullptr);

  m_alloc.destroy(m_offscreenDepth);
  m_sharedAlloc.destroy(m_offscreenColor);
  m_sharedAlloc.destroy(m_offscreenObjectId);
  m_sharedAlloc.destroy(m_offscreenInstanceId);
  m_sharedAlloc.destroy(m_offscreenDepthAov);
  m_sharedAlloc.deinit();

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
    refreshOffscreenRenderTargetsIfNeeded();
    return;
  }

  m_size.width  = w;
  m_size.height = h;
  createOffscreenRender();
  refreshOffscreenRenderTargetDescriptors();
  resetFrameHistory();
  m_hasLastCamera = false;
}

void HelloVulkan::createOffscreenRender()
{
  destroyRasterFramebuffer();

  m_renderSize = computeRenderSize();
  m_aovSize    = m_size;

  constexpr VkImageUsageFlags kAovUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  createOffscreenImage(m_offscreenColor, m_offscreenColorFormat,
                       kAovUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                       m_renderSize);
  createOffscreenImage(m_offscreenObjectId, m_offscreenObjectIdFormat, kAovUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       m_aovSize);
  createOffscreenImage(m_offscreenInstanceId, m_offscreenInstanceIdFormat, kAovUsage, m_aovSize);
  createOffscreenImage(m_offscreenDepthAov, m_offscreenDepthAovFormat, kAovUsage, m_aovSize);
  m_alloc.destroy(m_offscreenDepth);
  auto depthCreateInfo =
      nvvk::makeImage2DCreateInfo(m_renderSize, m_offscreenDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  {
    nvvk::Image image = m_alloc.createImage(depthCreateInfo);

    VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format           = m_offscreenDepthFormat;
    depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthStencilView.image            = image.image;

    m_offscreenDepth = m_alloc.createTexture(image, depthStencilView);
  }


  {
    nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
    auto              cmdBuf = genCmdBuf.createCommandBuffer();
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenObjectId.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenInstanceId.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenDepthAov.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    genCmdBuf.submitAndWait(cmdBuf);
  }

  createRasterFramebuffer();
  resetFrameHistory();
}
