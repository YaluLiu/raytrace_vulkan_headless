#include "features/tile/multiview_tile_raster_pipeline.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "nvh/fileoperations.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/shaders_vk.hpp"

#include <raster/mesh_types.hpp>

extern std::vector<std::string> defaultSearchPaths;

void MultiviewTileRasterPipeline::setup(VkDevice device,
                                        VkPhysicalDevice physicalDevice,
                                        uint32_t graphicsQueueIndex,
                                        nvvk::DebugUtil& debug)
{
  _device             = device;
  _physicalDevice     = physicalDevice;
  _graphicsQueueIndex = graphicsQueueIndex;
  _debug              = &debug;
}

void MultiviewTileRasterPipeline::destroy()
{
  destroyGraphicsPipeline();
  _device                   = VK_NULL_HANDLE;
  _physicalDevice           = VK_NULL_HANDLE;
  _graphicsQueueIndex       = 0;
  _debug                    = nullptr;
  _sceneDescriptorSetLayout = VK_NULL_HANDLE;
  _viewCount                = 0;
}

void MultiviewTileRasterPipeline::destroyGraphicsPipeline()
{
  if(_device != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(_device, _rasterPipeline, nullptr);
    vkDestroyPipeline(_device, _domeBackgroundPipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
    vkDestroyRenderPass(_device, _renderPass, nullptr);
  }
  _rasterPipeline         = VK_NULL_HANDLE;
  _domeBackgroundPipeline = VK_NULL_HANDLE;
  _pipelineLayout         = VK_NULL_HANDLE;
  _renderPass             = VK_NULL_HANDLE;
}

bool MultiviewTileRasterPipeline::ensureResources(VkDescriptorSetLayout sceneDescriptorSetLayout,
                                                  const PreviewRasterPipeline& referencePipeline,
                                                  uint32_t viewCount)
{
  if(_device == VK_NULL_HANDLE || sceneDescriptorSetLayout == VK_NULL_HANDLE || viewCount == 0)
  {
    return false;
  }

  const bool matches = _renderPass != VK_NULL_HANDLE && _rasterPipeline != VK_NULL_HANDLE
                       && _domeBackgroundPipeline != VK_NULL_HANDLE && _pipelineLayout != VK_NULL_HANDLE
                       && _sceneDescriptorSetLayout == sceneDescriptorSetLayout && _viewCount == viewCount
                       && _colorFormat == referencePipeline.getColorFormat()
                       && _depthAovFormat == referencePipeline.getDepthAovFormat()
                       && _depthAttachmentFormat == referencePipeline.getDepthAttachmentFormat();
  if(matches)
  {
    return true;
  }

  destroyGraphicsPipeline();
  _sceneDescriptorSetLayout = sceneDescriptorSetLayout;
  _viewCount                = viewCount;
  _colorFormat              = referencePipeline.getColorFormat();
  _depthAovFormat           = referencePipeline.getDepthAovFormat();
  _depthAttachmentFormat    = referencePipeline.getDepthAttachmentFormat();

  createRenderPass();
  createGraphicsPipeline();
  return _renderPass != VK_NULL_HANDLE && _rasterPipeline != VK_NULL_HANDLE
         && _domeBackgroundPipeline != VK_NULL_HANDLE && _pipelineLayout != VK_NULL_HANDLE;
}

void MultiviewTileRasterPipeline::createRenderPass()
{
  std::array<VkAttachmentDescription, 3> attachments{};
  const std::array<VkFormat, 2> colorFormats{_colorFormat, _depthAovFormat};
  for(size_t attachment = 0; attachment < colorFormats.size(); ++attachment)
  {
    attachments[attachment].format         = colorFormats[attachment];
    attachments[attachment].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[attachment].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[attachment].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[attachment].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[attachment].initialLayout  = VK_IMAGE_LAYOUT_GENERAL;
    attachments[attachment].finalLayout    = VK_IMAGE_LAYOUT_GENERAL;
  }

  attachments[2].format         = _depthAttachmentFormat;
  attachments[2].samples        = VK_SAMPLE_COUNT_1_BIT;
  attachments[2].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[2].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[2].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[2].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachments[2].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  std::array<VkAttachmentReference, 2> colorRefs{};
  for(uint32_t attachment = 0; attachment < colorRefs.size(); ++attachment)
  {
    colorRefs[attachment] = {attachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  }
  VkAttachmentReference depthRef{2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
  subpass.pColorAttachments       = colorRefs.data();
  subpass.pDepthStencilAttachment = &depthRef;

  std::array<VkSubpassDependency, 2> dependencies{};
  dependencies[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass    = 0;
  dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  dependencies[1].srcSubpass    = 0;
  dependencies[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

  const uint32_t viewMask = _viewCount >= 32 ? 0xFFFFFFFFu : ((1u << _viewCount) - 1u);
  VkRenderPassMultiviewCreateInfo multiview{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
  multiview.subpassCount        = 1;
  multiview.pViewMasks          = &viewMask;
  multiview.correlationMaskCount = 1;
  multiview.pCorrelationMasks    = &viewMask;

  VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassInfo.pNext           = &multiview;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments    = attachments.data();
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies   = dependencies.data();

  if(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create tile multiview render pass");
  }
}

void MultiviewTileRasterPipeline::createGraphicsPipeline()
{
  VkPushConstantRange pushConstant{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(PushConstantRaster)};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount         = 1;
  pipelineLayoutCreateInfo.pSetLayouts            = &_sceneDescriptorSetLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

  if(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create tile multiview pipeline layout");
  }

  std::vector<VkFormat> colorAttachmentFormats{_colorFormat, _depthAovFormat};

  nvvk::GraphicsPipelineState pipelineState;
  pipelineState.rasterizationState.cullMode = VK_CULL_MODE_NONE;
  pipelineState.setBlendAttachmentCount(static_cast<uint32_t>(colorAttachmentFormats.size()));
  for(uint32_t attachment = 0; attachment < colorAttachmentFormats.size(); ++attachment)
  {
    pipelineState.setBlendAttachmentState(attachment,
                                          nvvk::GraphicsPipelineState::makePipelineColorBlendAttachmentState());
  }
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

  nvvk::GraphicsPipelineGenerator pipelineGenerator(_device, _pipelineLayout, _renderPass, pipelineState);
  pipelineGenerator.addShader(nvh::loadFile("spv/tile_multiview.vert.spv", true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(nvh::loadFile("spv/tile_multiview.frag.spv", true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_FRAGMENT_BIT);
  _rasterPipeline = pipelineGenerator.createPipeline();
  if(_rasterPipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error("Failed to create tile multiview raster pipeline");
  }

  nvvk::GraphicsPipelineState backgroundState;
  backgroundState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
  backgroundState.depthStencilState.depthTestEnable  = VK_FALSE;
  backgroundState.depthStencilState.depthWriteEnable = VK_FALSE;
  backgroundState.setBlendAttachmentCount(static_cast<uint32_t>(colorAttachmentFormats.size()));
  for(uint32_t attachment = 0; attachment < colorAttachmentFormats.size(); ++attachment)
  {
    backgroundState.setBlendAttachmentState(attachment,
                                            nvvk::GraphicsPipelineState::makePipelineColorBlendAttachmentState());
  }

  nvvk::GraphicsPipelineGenerator backgroundGenerator(_device, _pipelineLayout, _renderPass, backgroundState);
  backgroundGenerator.addShader(nvh::loadFile("spv/dome_background_tile_multiview.vert.spv", true, defaultSearchPaths, true),
                                VK_SHADER_STAGE_VERTEX_BIT);
  backgroundGenerator.addShader(nvh::loadFile("spv/dome_background_tile_multiview.frag.spv", true, defaultSearchPaths, true),
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  _domeBackgroundPipeline = backgroundGenerator.createPipeline();
  if(_domeBackgroundPipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error("Failed to create tile multiview background pipeline");
  }
}

bool MultiviewTileRasterPipeline::rasterize(const VkCommandBuffer& cmdBuf,
                                            VkFramebuffer framebuffer,
                                            VkExtent2D tileExtent,
                                            VkDescriptorSet sceneDescriptorSet,
                                            std::span<const RasterMeshBuffers> objModels,
                                            std::span<const RasterInstance> instances,
                                            std::span<const int> instanceIds)
{
  if(_rasterPipeline == VK_NULL_HANDLE || _domeBackgroundPipeline == VK_NULL_HANDLE || _pipelineLayout == VK_NULL_HANDLE
     || _renderPass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE || sceneDescriptorSet == VK_NULL_HANDLE
     || tileExtent.width == 0 || tileExtent.height == 0)
  {
    return false;
  }

  if(_debug != nullptr)
  {
    _debug->beginLabel(cmdBuf, "TileMultiviewRasterize");
  }

  std::array<VkClearValue, 3> clearValues{};
  clearValues[0].color.float32[0] = 0.0f;
  clearValues[0].color.float32[1] = 0.0f;
  clearValues[0].color.float32[2] = 0.0f;
  clearValues[0].color.float32[3] = 1.0f;
  clearValues[1].color.float32[0] = 1.0f;
  clearValues[2].depthStencil     = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass      = _renderPass;
  renderPassInfo.framebuffer     = framebuffer;
  renderPassInfo.renderArea      = {{0, 0}, tileExtent};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues    = clearValues.data();

  vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{0.0f, 0.0f, static_cast<float>(tileExtent.width), static_cast<float>(tileExtent.height), 0.0f,
                      1.0f};
  VkRect2D scissor{{0, 0}, tileExtent};
  vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  const uint32_t frameUniformOffset = 0;
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _domeBackgroundPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &sceneDescriptorSet, 1,
                          &frameUniformOffset);
  vkCmdDraw(cmdBuf, 3, 1, 0, 0);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &sceneDescriptorSet, 1,
                          &frameUniformOffset);

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

    PushConstantRaster pushConstant{};
    pushConstant.model      = instance.transform;
    pushConstant.objIndex   = instance.objIndex;
    pushConstant.instanceId =
        instanceIndex < instanceIds.size() ? instanceIds[instanceIndex] : static_cast<int>(instanceIndex);
    vkCmdPushConstants(cmdBuf, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstantRaster), &pushConstant);
    vkCmdDrawIndexed(cmdBuf, model.nbIndices, 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(cmdBuf);

  if(_debug != nullptr)
  {
    _debug->endLabel(cmdBuf);
  }
  return true;
}
