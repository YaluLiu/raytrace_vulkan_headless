#include "output/lidar/lidar_depth_pipeline.hpp"

#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "shaders/common/host_device.h"

#include <raster/mesh_types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

extern std::vector<std::string> defaultSearchPaths;

void LidarDepthPipeline::setup(VkDevice device,
                               VkPhysicalDevice physicalDevice,
                               uint32_t graphicsQueueIndex,
                               nvvk::ResourceAllocatorDma& allocator,
                               nvvk::DebugUtil& debug)
{
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_allocator = &allocator;
  m_debug = &debug;
  m_depthFormat = nvvk::findDepthFormat(physicalDevice);
}

void LidarDepthPipeline::destroy()
{
  destroyGraphicsPipeline();
  destroyImages();
  m_device = VK_NULL_HANDLE;
  m_physicalDevice = VK_NULL_HANDLE;
  m_graphicsQueueIndex = 0;
  m_allocator = nullptr;
  m_debug = nullptr;
  m_sceneDescriptorSetLayout = VK_NULL_HANDLE;
}

void LidarDepthPipeline::destroyGraphicsPipeline()
{
  destroyFramebuffer();
  if(m_device != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
  }
  m_pipeline = VK_NULL_HANDLE;
  m_pipelineLayout = VK_NULL_HANDLE;
  m_renderPass = VK_NULL_HANDLE;
}

bool LidarDepthPipeline::ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout, VkExtent2D maxExtent)
{
  if(m_device == VK_NULL_HANDLE || m_allocator == nullptr || sceneDescriptorSetLayout == VK_NULL_HANDLE ||
     maxExtent.width == 0 || maxExtent.height == 0)
  {
    return false;
  }

  const bool hasImages = m_rangeImage.image != VK_NULL_HANDLE && m_rangeImage.descriptor.imageView != VK_NULL_HANDLE &&
                         m_depthImage.image != VK_NULL_HANDLE && m_depthImage.descriptor.imageView != VK_NULL_HANDLE;
  const bool imageMatches = hasImages && maxExtent.width <= m_extent.width && maxExtent.height <= m_extent.height;
  const bool pipelineMatches = m_renderPass != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE &&
                               m_pipelineLayout != VK_NULL_HANDLE && m_sceneDescriptorSetLayout == sceneDescriptorSetLayout;
  if(imageMatches && pipelineMatches && m_framebuffer != VK_NULL_HANDLE)
  {
    return true;
  }

  if(!imageMatches)
  {
    createImages(maxExtent);
  }

  if(!pipelineMatches)
  {
    destroyGraphicsPipeline();
    m_sceneDescriptorSetLayout = sceneDescriptorSetLayout;
    createRenderPass();
    createGraphicsPipeline();
  }
  else
  {
    destroyFramebuffer();
  }
  createFramebuffer();
  return m_rangeImage.image != VK_NULL_HANDLE && m_depthImage.image != VK_NULL_HANDLE && m_renderPass != VK_NULL_HANDLE &&
         m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE && m_framebuffer != VK_NULL_HANDLE;
}

bool LidarDepthPipeline::rasterize(const VkCommandBuffer& cmdBuf,
                                   const RasterLidarSensorSpec& sensor,
                                   const RasterLidarSensorMetadata& metadata,
                                   VkDescriptorSet sceneDescriptorSet,
                                   std::span<const RasterMeshBuffers> objModels,
                                   std::span<const RasterInstance> instances,
                                   std::span<const int> instanceIds)
{
  if(m_renderPass == VK_NULL_HANDLE || m_framebuffer == VK_NULL_HANDLE || m_pipeline == VK_NULL_HANDLE ||
     m_pipelineLayout == VK_NULL_HANDLE || sceneDescriptorSet == VK_NULL_HANDLE || metadata.azimuthSampleCount == 0 ||
     metadata.verticalSampleCount == 0)
  {
    return false;
  }

  const VkExtent2D sensorExtent{metadata.azimuthSampleCount, metadata.verticalSampleCount};
  if(sensorExtent.width > m_extent.width || sensorExtent.height > m_extent.height)
  {
    return false;
  }

  if(m_debug != nullptr)
  {
    m_debug->beginLabel(cmdBuf, "LidarDepthRasterize");
  }

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color.float32[0] = 0.0f;
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass = m_renderPass;
  renderPassInfo.framebuffer = m_framebuffer;
  renderPassInfo.renderArea = {{0, 0}, sensorExtent};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{0.0f, 0.0f, static_cast<float>(sensorExtent.width), static_cast<float>(sensorExtent.height),
                      0.0f, 1.0f};
  VkRect2D scissor{{0, 0}, sensorExtent};
  vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  const uint32_t frameUniformOffset = 0;
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &sceneDescriptorSet, 1,
                          &frameUniformOffset);

  const RasterLidarBasis basis = BuildLidarBasis(sensor);
  for(size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
  {
    const RasterInstance& instance = instances[instanceIndex];
    if(!instance.visible || instance.objIndex >= objModels.size())
    {
      continue;
    }

    const RasterMeshBuffers& model = objModels[instance.objIndex];
    if(model.nbIndices == 0 || model.vertexBuffer.buffer == VK_NULL_HANDLE || model.indexBuffer.buffer == VK_NULL_HANDLE)
    {
      continue;
    }

    VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &model.vertexBuffer.buffer, &vertexOffset);
    vkCmdBindIndexBuffer(cmdBuf, model.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    PushConstantLidarDepth pushConstant{};
    pushConstant.model = instance.transform;
    pushConstant.originMaxRange = vec4(basis.origin, metadata.maxRange);
    pushConstant.forwardAzimuthStart = vec4(basis.forward, sensor.params.azimuthStartDeg);
    pushConstant.rightAzimuthStep = vec4(basis.right, std::max(std::fabs(sensor.params.azimuthStepDeg), 1.0e-4f));
    pushConstant.upVerticalStart = vec4(basis.up, sensor.params.verticalStartDeg);
    pushConstant.azimuthEndVerticalEndStep =
        vec4(sensor.params.azimuthEndDeg, sensor.params.verticalEndDeg,
             std::max(std::fabs(sensor.params.verticalStepDeg), 1.0e-4f), 0.0f);
    pushConstant.objIndex = instance.objIndex;
    pushConstant.sensorIndex = metadata.sensorIndex;
    pushConstant.azimuthSampleCount = metadata.azimuthSampleCount;
    pushConstant.verticalSampleCount = metadata.verticalSampleCount;
    vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantLidarDepth),
                       &pushConstant);
    vkCmdDrawIndexed(cmdBuf, model.nbIndices, 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(cmdBuf);
  if(m_debug != nullptr)
  {
    m_debug->endLabel(cmdBuf);
  }
  return true;
}

void LidarDepthPipeline::createImages(VkExtent2D extent)
{
  destroyFramebuffer();
  destroyImages();
  m_extent = extent;

  const VkImageUsageFlags rangeUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  auto rangeInfo = nvvk::makeImage2DCreateInfo(extent, m_rangeFormat, rangeUsage);
  nvvk::Image rangeImage = m_allocator->createImage(rangeInfo);
  VkImageViewCreateInfo rangeViewInfo = nvvk::makeImageViewCreateInfo(rangeImage.image, rangeInfo);
  m_rangeImage = m_allocator->createTexture(rangeImage, rangeViewInfo);
  m_rangeImage.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  auto depthInfo =
      nvvk::makeImage2DCreateInfo(extent, m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  nvvk::Image depthImage = m_allocator->createImage(depthInfo);
  VkImageViewCreateInfo depthViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthViewInfo.format = m_depthFormat;
  depthViewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  depthViewInfo.image = depthImage.image;
  m_depthImage = m_allocator->createTexture(depthImage, depthViewInfo);
  m_depthImage.descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  nvvk::CommandPool commandPool(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();
  nvvk::cmdBarrierImageLayout(cmdBuf, m_rangeImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  nvvk::cmdBarrierImageLayout(cmdBuf, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
  commandPool.submitAndWait(cmdBuf);

  if(m_debug != nullptr)
  {
    m_debug->setObjectName(m_rangeImage.image, "LidarRangeImage");
    m_debug->setObjectName(m_depthImage.image, "LidarDepthAttachment");
  }
}

void LidarDepthPipeline::createRenderPass()
{
  std::array<VkAttachmentDescription, 2> attachments{};
  attachments[0].format = m_rangeFormat;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

  attachments[1].format = m_depthFormat;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  std::array<VkSubpassDependency, 2> dependencies{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  dependencies[0].dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();
  if(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create LiDAR depth render pass");
  }
}

void LidarDepthPipeline::createFramebuffer()
{
  if(m_renderPass == VK_NULL_HANDLE || m_rangeImage.descriptor.imageView == VK_NULL_HANDLE ||
     m_depthImage.descriptor.imageView == VK_NULL_HANDLE || m_extent.width == 0 || m_extent.height == 0)
  {
    return;
  }
  destroyFramebuffer();

  std::array<VkImageView, 2> attachments{m_rangeImage.descriptor.imageView, m_depthImage.descriptor.imageView};
  VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebufferInfo.renderPass = m_renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = m_extent.width;
  framebufferInfo.height = m_extent.height;
  framebufferInfo.layers = 1;
  if(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create LiDAR depth framebuffer");
  }
}

void LidarDepthPipeline::createGraphicsPipeline()
{
  VkPushConstantRange pushConstant{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantLidarDepth)};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts = &m_sceneDescriptorSetLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
  if(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create LiDAR depth pipeline layout");
  }

  nvvk::GraphicsPipelineState pipelineState;
  pipelineState.rasterizationState.cullMode = VK_CULL_MODE_NONE;
  pipelineState.setBlendAttachmentCount(1);
  pipelineState.setBlendAttachmentState(0, nvvk::GraphicsPipelineState::makePipelineColorBlendAttachmentState());
  pipelineState.addBindingDescription(
      nvvk::GraphicsPipelineState::makeVertexInputBinding(0, sizeof(RasterVertex), VK_VERTEX_INPUT_RATE_VERTEX));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RasterVertex, pos)));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RasterVertex, nrm)));
  pipelineState.addAttributeDescription(nvvk::GraphicsPipelineState::makeVertexInputAttribute(
      2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RasterVertex, color)));
  pipelineState.addAttributeDescription(nvvk::GraphicsPipelineState::makeVertexInputAttribute(
      3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(RasterVertex, texCoord)));
  pipelineState.addAttributeDescription(nvvk::GraphicsPipelineState::makeVertexInputAttribute(
      4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RasterVertex, tangent)));

  nvvk::GraphicsPipelineGenerator pipelineGenerator(m_device, m_pipelineLayout, m_renderPass, pipelineState);
  pipelineGenerator.addShader(nvh::loadFile("spv/lidar_depth.vert.spv", true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(nvh::loadFile("spv/lidar_depth.frag.spv", true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pipeline = pipelineGenerator.createPipeline();
  if(m_pipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error("Failed to create LiDAR depth graphics pipeline");
  }
}

void LidarDepthPipeline::destroyImages()
{
  if(m_allocator != nullptr)
  {
    m_allocator->destroy(m_rangeImage);
    m_allocator->destroy(m_depthImage);
  }
  m_rangeImage = {};
  m_depthImage = {};
  m_extent = {0, 0};
}

void LidarDepthPipeline::destroyFramebuffer()
{
  if(m_device != VK_NULL_HANDLE && m_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }
}
