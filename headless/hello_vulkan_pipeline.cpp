#include <array>
#include <cstddef>
#include <stdexcept>
#include "hello_vulkan.hpp"
#include "hello_vulkan_barriers.hpp"
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
  m_descSetLayoutBind.addBinding(SceneBindings::eLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
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

  VkDescriptorBufferInfo dbiLights{m_bLights.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eLights, &dbiLights));

  std::vector<VkDescriptorImageInfo> diit;
  for(auto& texture : m_textures)
  {
    diit.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_descSetLayoutBind.makeWriteArray(m_descSet, SceneBindings::eTextures, diit.data()));

  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
