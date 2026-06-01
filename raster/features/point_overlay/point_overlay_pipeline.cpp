#include "features/point_overlay/point_overlay_pipeline.hpp"

#include "nvh/fileoperations.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "features/preview/preview_raster_pipeline.hpp"
#include "shaders/common/host_device.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

extern std::vector<std::string> defaultSearchPaths;

namespace {

std::string MakeError(const PointOverlayPipelineConfig& config, const char* action)
{
  return "Failed to " + std::string(action) + " " + config.errorPrefix;
}

}  // namespace

void PointOverlayPipeline::setup(VkDevice device, nvvk::DebugUtil& debug, PointOverlayPipelineConfig config)
{
  m_device = device;
  m_debug = &debug;
  m_config = std::move(config);
}

void PointOverlayPipeline::destroy()
{
  destroyGraphicsPipeline();
  if(m_device != VK_NULL_HANDLE)
  {
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
  }
  m_descriptorPool = VK_NULL_HANDLE;
  m_descriptorSetLayout = VK_NULL_HANDLE;
  m_descriptorSet = VK_NULL_HANDLE;
  m_boundSensorBuffer = VK_NULL_HANDLE;
  m_boundPointBuffer = VK_NULL_HANDLE;
  m_device = VK_NULL_HANDLE;
  m_debug = nullptr;
  m_config = {};
}

void PointOverlayPipeline::destroyGraphicsPipeline()
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

bool PointOverlayPipeline::ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                           const PreviewRasterPipeline& previewPipeline,
                                           VkBuffer sensorBuffer,
                                           VkBuffer pointBuffer)
{
  if(m_device == VK_NULL_HANDLE || sceneDescriptorSetLayout == VK_NULL_HANDLE || sensorBuffer == VK_NULL_HANDLE ||
     pointBuffer == VK_NULL_HANDLE || previewPipeline.getColorImageView() == VK_NULL_HANDLE ||
     previewPipeline.getDepthAttachmentImageView() == VK_NULL_HANDLE)
  {
    return false;
  }

  if(m_descriptorSetLayout == VK_NULL_HANDLE)
  {
    createDescriptorResources();
  }
  if(m_boundSensorBuffer != sensorBuffer || m_boundPointBuffer != pointBuffer)
  {
    updateDescriptorSet(sensorBuffer, pointBuffer);
  }

  const bool pipelineMatches = m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE &&
                               m_renderPass != VK_NULL_HANDLE &&
                               m_sceneDescriptorSetLayout == sceneDescriptorSetLayout &&
                               m_colorFormat == previewPipeline.getColorFormat() &&
                               m_depthFormat == previewPipeline.getDepthAttachmentFormat();
  const VkExtent2D renderSize = previewPipeline.getRenderSize();
  const bool framebufferMatches = m_framebuffer != VK_NULL_HANDLE && m_extent.width == renderSize.width &&
                                  m_extent.height == renderSize.height;
  if(!pipelineMatches)
  {
    destroyGraphicsPipeline();
    m_sceneDescriptorSetLayout = sceneDescriptorSetLayout;
    m_colorFormat = previewPipeline.getColorFormat();
    m_depthFormat = previewPipeline.getDepthAttachmentFormat();
    createRenderPass();
    createGraphicsPipeline();
  }
  if(!framebufferMatches || !pipelineMatches)
  {
    createFramebuffer(previewPipeline);
  }

  return m_descriptorSet != VK_NULL_HANDLE && m_renderPass != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE &&
         m_pipelineLayout != VK_NULL_HANDLE && m_framebuffer != VK_NULL_HANDLE;
}

void PointOverlayPipeline::draw(const VkCommandBuffer& cmdBuf,
                                const PreviewRasterPipeline& previewPipeline,
                                VkDescriptorSet sceneDescriptorSet,
                                uint32_t sensorIndex,
                                float pointSizePixels,
                                uint32_t vertexCount)
{
  if(vertexCount == 0 || pointSizePixels <= 0.0f || m_renderPass == VK_NULL_HANDLE ||
     m_framebuffer == VK_NULL_HANDLE || m_pipeline == VK_NULL_HANDLE || m_pipelineLayout == VK_NULL_HANDLE ||
     m_descriptorSet == VK_NULL_HANDLE || sceneDescriptorSet == VK_NULL_HANDLE)
  {
    return;
  }

  if(m_debug != nullptr && !m_config.debugLabel.empty())
  {
    m_debug->beginLabel(cmdBuf, m_config.debugLabel.c_str());
  }

  std::array<VkClearValue, 2> clearValues{};
  VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass = m_renderPass;
  renderPassInfo.framebuffer = m_framebuffer;
  renderPassInfo.renderArea = {{0, 0}, previewPipeline.getRenderSize()};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{0.0f, 0.0f, static_cast<float>(previewPipeline.getRenderSize().width),
                      static_cast<float>(previewPipeline.getRenderSize().height), 0.0f, 1.0f};
  VkRect2D scissor{{0, 0}, previewPipeline.getRenderSize()};
  vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  std::array<VkDescriptorSet, 2> descriptorSets{sceneDescriptorSet, m_descriptorSet};
  const uint32_t frameUniformOffset = 0;
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0,
                          static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 1,
                          &frameUniformOffset);

  PushConstantPointOverlay pushConstant{};
  pushConstant.sensorIndex = sensorIndex;
  pushConstant.pointSizePixels = pointSizePixels;
  vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(pushConstant), &pushConstant);
  vkCmdDraw(cmdBuf, vertexCount, 1, 0, 0);
  vkCmdEndRenderPass(cmdBuf);

  if(m_debug != nullptr && !m_config.debugLabel.empty())
  {
    m_debug->endLabel(cmdBuf);
  }
}

void PointOverlayPipeline::createDescriptorResources()
{
  m_bindings.clear();
  m_bindings.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
  m_bindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
  m_descriptorSetLayout = m_bindings.createLayout(m_device);
  m_descriptorPool = m_bindings.createPool(m_device, 1);
  m_descriptorSet = nvvk::allocateDescriptorSet(m_device, m_descriptorPool, m_descriptorSetLayout);
}

void PointOverlayPipeline::updateDescriptorSet(VkBuffer sensorBuffer, VkBuffer pointBuffer)
{
  VkDescriptorBufferInfo sensors{sensorBuffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo points{pointBuffer, 0, VK_WHOLE_SIZE};
  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_bindings.makeWrite(m_descriptorSet, 0, &sensors));
  writes.emplace_back(m_bindings.makeWrite(m_descriptorSet, 1, &points));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
  m_boundSensorBuffer = sensorBuffer;
  m_boundPointBuffer = pointBuffer;
}

void PointOverlayPipeline::createRenderPass()
{
  std::array<VkAttachmentDescription, 2> attachments{};
  attachments[0].format = m_colorFormat;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

  attachments[1].format = m_depthFormat;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();
  if(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
  {
    throw std::runtime_error(MakeError(m_config, "create render pass"));
  }
}

void PointOverlayPipeline::createFramebuffer(const PreviewRasterPipeline& previewPipeline)
{
  destroyFramebuffer();
  m_extent = previewPipeline.getRenderSize();
  if(m_renderPass == VK_NULL_HANDLE || m_extent.width == 0 || m_extent.height == 0)
  {
    return;
  }

  std::array<VkImageView, 2> attachments{previewPipeline.getColorImageView(),
                                         previewPipeline.getDepthAttachmentImageView()};
  VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebufferInfo.renderPass = m_renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = m_extent.width;
  framebufferInfo.height = m_extent.height;
  framebufferInfo.layers = 1;
  if(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
  {
    throw std::runtime_error(MakeError(m_config, "create framebuffer"));
  }
}

void PointOverlayPipeline::createGraphicsPipeline()
{
  std::array<VkDescriptorSetLayout, 2> setLayouts{m_sceneDescriptorSetLayout, m_descriptorSetLayout};
  VkPushConstantRange pushConstant{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(PushConstantPointOverlay)};
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
  if(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error(MakeError(m_config, "create pipeline layout"));
  }

  nvvk::GraphicsPipelineState pipelineState;
  pipelineState.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  pipelineState.rasterizationState.cullMode = VK_CULL_MODE_NONE;
  pipelineState.depthStencilState.depthTestEnable = VK_TRUE;
  pipelineState.depthStencilState.depthWriteEnable = VK_FALSE;
  pipelineState.setBlendAttachmentCount(1);
  pipelineState.setBlendAttachmentState(0, nvvk::GraphicsPipelineState::makePipelineColorBlendAttachmentState());

  nvvk::GraphicsPipelineGenerator pipelineGenerator(m_device, m_pipelineLayout, m_renderPass, pipelineState);
  pipelineGenerator.addShader(nvh::loadFile(m_config.vertexShaderSpvPath, true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(nvh::loadFile(m_config.fragmentShaderSpvPath, true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pipeline = pipelineGenerator.createPipeline();
  if(m_pipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error(MakeError(m_config, "create graphics pipeline"));
  }
}

void PointOverlayPipeline::destroyFramebuffer()
{
  if(m_device != VK_NULL_HANDLE && m_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }
}
