#include "raster_pipeline.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "data_loader.h"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/shaders_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;

namespace {
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

void RasterPipeline::setup(VkDevice device,
                           VkPhysicalDevice physicalDevice,
                           uint32_t graphicsQueueIndex,
                           nvvk::ResourceAllocatorDma& transientAllocator,
                           nvvk::DebugUtil& debug)
{
  _device             = device;
  _physicalDevice     = physicalDevice;
  _graphicsQueueIndex = graphicsQueueIndex;
  _transientAllocator = &transientAllocator;
  _debug              = &debug;
  _offscreenDepthFormat = nvvk::findDepthFormat(physicalDevice);
  _sharedAlloc.init(device, physicalDevice);
}

void RasterPipeline::destroy()
{
  destroyGraphicsPipeline();
  if(_transientAllocator != nullptr)
  {
    _transientAllocator->destroy(_offscreenDepth);
    _offscreenDepth = {};
  }
  if(_device != VK_NULL_HANDLE)
  {
    _sharedAlloc.destroy(_offscreenColor);
    _sharedAlloc.destroy(_offscreenObjectId);
    _sharedAlloc.destroy(_offscreenInstanceId);
    _sharedAlloc.destroy(_offscreenDepthAov);
    _sharedAlloc.deinit();
    _offscreenColor      = {};
    _offscreenObjectId   = {};
    _offscreenInstanceId = {};
    _offscreenDepthAov   = {};
  }
  _device             = VK_NULL_HANDLE;
  _physicalDevice     = VK_NULL_HANDLE;
  _graphicsQueueIndex = 0;
  _transientAllocator = nullptr;
  _debug              = nullptr;
  _renderSize         = {0, 0};
  _aovSize            = {0, 0};
}

void RasterPipeline::destroyGraphicsPipeline()
{
  destroyFramebuffer();
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

void RasterPipeline::createOffscreenRender(VkExtent2D size)
{
  if(_device == VK_NULL_HANDLE || _transientAllocator == nullptr || size.width == 0 || size.height == 0)
  {
    return;
  }

  destroyFramebuffer();

  _renderSize = size;
  _aovSize    = size;

  constexpr VkImageUsageFlags kAovUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  createOffscreenImage(_offscreenColor, _offscreenColorFormat,
                       kAovUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _renderSize);
  createOffscreenImage(_offscreenObjectId, _offscreenObjectIdFormat, kAovUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       _aovSize);
  createOffscreenImage(_offscreenInstanceId, _offscreenInstanceIdFormat, kAovUsage, _aovSize);
  createOffscreenImage(_offscreenDepthAov, _offscreenDepthAovFormat, kAovUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       _aovSize);

  _transientAllocator->destroy(_offscreenDepth);
  auto depthCreateInfo =
      nvvk::makeImage2DCreateInfo(_renderSize, _offscreenDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  {
    nvvk::Image image = _transientAllocator->createImage(depthCreateInfo);

    VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format           = _offscreenDepthFormat;
    depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthStencilView.image            = image.image;

    _offscreenDepth = _transientAllocator->createTexture(image, depthStencilView);
  }

  {
    nvvk::CommandPool genCmdBuf(_device, _graphicsQueueIndex);
    auto              cmdBuf = genCmdBuf.createCommandBuffer();
    nvvk::cmdBarrierImageLayout(cmdBuf, _offscreenColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, _offscreenDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    nvvk::cmdBarrierImageLayout(cmdBuf, _offscreenObjectId.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, _offscreenInstanceId.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, _offscreenDepthAov.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    genCmdBuf.submitAndWait(cmdBuf);
  }

  createFramebuffer();
}

void RasterPipeline::createGraphicsPipeline(VkDescriptorSetLayout sceneDescriptorSetLayout)
{
  destroyGraphicsPipeline();

  VkPushConstantRange pushConstant{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(PushConstantRaster)};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount         = 1;
  pipelineLayoutCreateInfo.pSetLayouts            = &sceneDescriptorSetLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

  if(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create raster pipeline layout");
  }

  std::vector<VkFormat> colorAttachmentFormats{
      _offscreenColorFormat, _offscreenObjectIdFormat, _offscreenInstanceIdFormat, _offscreenDepthAovFormat};
  _renderPass = nvvk::createRenderPass(_device, colorAttachmentFormats, _offscreenDepthFormat, 1, true, true,
                                       VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);

  nvvk::GraphicsPipelineState pipelineState;
  pipelineState.rasterizationState.cullMode = VK_CULL_MODE_NONE;
  pipelineState.setBlendAttachmentCount(static_cast<uint32_t>(colorAttachmentFormats.size()));
  for(uint32_t attachment = 0; attachment < colorAttachmentFormats.size(); ++attachment)
  {
    pipelineState.setBlendAttachmentState(attachment,
                                          nvvk::GraphicsPipelineState::makePipelineColorBlendAttachmentState());
  }
  pipelineState.addBindingDescription(
      nvvk::GraphicsPipelineState::makeVertexInputBinding(0, sizeof(VertexObj), VK_VERTEX_INPUT_RATE_VERTEX));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexObj, pos)));
  pipelineState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexObj, nrm)));
  pipelineState.addAttributeDescription(nvvk::GraphicsPipelineState::makeVertexInputAttribute(
      2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexObj, color)));
  pipelineState.addAttributeDescription(nvvk::GraphicsPipelineState::makeVertexInputAttribute(
      3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexObj, texCoord)));
  pipelineState.addAttributeDescription(nvvk::GraphicsPipelineState::makeVertexInputAttribute(
      4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexObj, tangent)));

  nvvk::GraphicsPipelineGenerator pipelineGenerator(_device, _pipelineLayout, _renderPass, pipelineState);
  pipelineGenerator.addShader(nvh::loadFile("spv/raster.vert.spv", true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(nvh::loadFile("spv/raster.frag.spv", true, defaultSearchPaths, true),
                              VK_SHADER_STAGE_FRAGMENT_BIT);
  _rasterPipeline = pipelineGenerator.createPipeline();
  if(_rasterPipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error("Failed to create raster graphics pipeline");
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
  backgroundGenerator.addShader(nvh::loadFile("spv/dome_background.vert.spv", true, defaultSearchPaths, true),
                                VK_SHADER_STAGE_VERTEX_BIT);
  backgroundGenerator.addShader(nvh::loadFile("spv/dome_background.frag.spv", true, defaultSearchPaths, true),
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  _domeBackgroundPipeline = backgroundGenerator.createPipeline();
  if(_domeBackgroundPipeline == VK_NULL_HANDLE)
  {
    throw std::runtime_error("Failed to create DomeLight background graphics pipeline");
  }

  createFramebuffer();
}

void RasterPipeline::rasterize(const VkCommandBuffer& cmdBuf,
                               VkDescriptorSet sceneDescriptorSet,
                               std::span<const RasterObjModel> objModels,
                               std::span<const RasterObjInstance> instances,
                               std::span<const int> instanceIds,
                               uint32_t frameUniformOffset)
{
  VkViewport viewport{0.0f, 0.0f, static_cast<float>(_aovSize.width), static_cast<float>(_aovSize.height), 0.0f,
                      1.0f};
  VkRect2D   scissor{{0, 0}, _aovSize};
  rasterizeWithTarget(cmdBuf, _framebuffer, _aovSize, scissor, viewport, scissor, sceneDescriptorSet, objModels,
                      instances, instanceIds, frameUniformOffset);
}

void RasterPipeline::rasterizeToFramebuffer(const VkCommandBuffer& cmdBuf,
                                            VkFramebuffer framebuffer,
                                            VkExtent2D framebufferExtent,
                                            VkRect2D renderArea,
                                            VkViewport viewport,
                                            VkRect2D scissor,
                                            VkDescriptorSet sceneDescriptorSet,
                                            std::span<const RasterObjModel> objModels,
                                            std::span<const RasterObjInstance> instances,
                                            std::span<const int> instanceIds,
                                            uint32_t frameUniformOffset)
{
  rasterizeWithTarget(cmdBuf, framebuffer, framebufferExtent, renderArea, viewport, scissor, sceneDescriptorSet,
                      objModels, instances, instanceIds, frameUniformOffset);
}

void RasterPipeline::rasterizeWithTarget(const VkCommandBuffer& cmdBuf,
                                         VkFramebuffer framebuffer,
                                         VkExtent2D framebufferExtent,
                                         VkRect2D renderArea,
                                         VkViewport viewport,
                                         VkRect2D scissor,
                                         VkDescriptorSet sceneDescriptorSet,
                                         std::span<const RasterObjModel> objModels,
                                         std::span<const RasterObjInstance> instances,
                                         std::span<const int> instanceIds,
                                         uint32_t frameUniformOffset)
{
  if(_rasterPipeline == VK_NULL_HANDLE || _domeBackgroundPipeline == VK_NULL_HANDLE || _pipelineLayout == VK_NULL_HANDLE
     || framebuffer == VK_NULL_HANDLE || sceneDescriptorSet == VK_NULL_HANDLE || _debug == nullptr
     || framebufferExtent.width == 0 || framebufferExtent.height == 0 || renderArea.extent.width == 0
     || renderArea.extent.height == 0)
  {
    return;
  }
  if(renderArea.offset.x < 0 || renderArea.offset.y < 0
     || static_cast<uint32_t>(renderArea.offset.x) + renderArea.extent.width > framebufferExtent.width
     || static_cast<uint32_t>(renderArea.offset.y) + renderArea.extent.height > framebufferExtent.height)
  {
    return;
  }

  _debug->beginLabel(cmdBuf, "Rasterize");

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
  renderPassInfo.renderPass        = _renderPass;
  renderPassInfo.framebuffer       = framebuffer;
  renderPassInfo.renderArea        = renderArea;
  renderPassInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues      = clearValues.data();

  vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _domeBackgroundPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &sceneDescriptorSet, 1,
                          &frameUniformOffset);
  vkCmdDraw(cmdBuf, 3, 1, 0, 0);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _rasterPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &sceneDescriptorSet, 1,
                          &frameUniformOffset);

  for(size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
  {
    const RasterObjInstance& instance = instances[instanceIndex];
    if(!instance.visible || instance.objIndex >= objModels.size())
    {
      continue;
    }

    const RasterObjModel& model = objModels[instance.objIndex];
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

  _debug->endLabel(cmdBuf);
}

std::optional<HeadlessAovTexture> RasterPipeline::getAovTexture(HeadlessAov aov) const
{
  const nvvk::Texture* texture = nullptr;
  VkFormat             format  = VK_FORMAT_UNDEFINED;
  VkExtent2D           extent  = {0, 0};

  switch(aov)
  {
    case HeadlessAov::Color:
      texture = &_offscreenColor;
      format  = _offscreenColorFormat;
      extent  = _renderSize;
      break;
    case HeadlessAov::PrimId:
      texture = &_offscreenObjectId;
      format  = _offscreenObjectIdFormat;
      extent  = _aovSize;
      break;
    case HeadlessAov::InstanceId:
      texture = &_offscreenInstanceId;
      format  = _offscreenInstanceIdFormat;
      extent  = _aovSize;
      break;
    case HeadlessAov::Depth:
      texture = &_offscreenDepthAov;
      format  = _offscreenDepthAovFormat;
      extent  = _aovSize;
      break;
    case HeadlessAov::TileColor:
    case HeadlessAov::TileDepth:
    case HeadlessAov::TileDisplayColor:
    case HeadlessAov::TileDisplayDepth:
      return std::nullopt;
  }

  if(texture == nullptr || texture->image == VK_NULL_HANDLE || texture->descriptor.imageView == VK_NULL_HANDLE
     || texture->memHandle == nullptr)
  {
    return std::nullopt;
  }

  auto* memAlloc = _sharedAlloc.getMemoryAllocator();
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
  result.device          = _device;
  result.image           = texture->image;
  result.imageView       = texture->descriptor.imageView;
  result.memory          = memoryInfo.memory;
  result.memoryOffset    = memoryInfo.offset;
  result.memorySize      = memoryInfo.size;
  result.format          = format;
  result.extent          = extent;
  result.layout          = texture->descriptor.imageLayout;
  result.dedicatedMemory = requiresDedicatedImageAllocation(_device, texture->image);
  return result;
}

void RasterPipeline::createOffscreenImage(nvvk::Texture& texture,
                                          VkFormat format,
                                          VkImageUsageFlags usage,
                                          VkExtent2D extent)
{
  if(extent.width == 0 || extent.height == 0)
  {
    extent = _renderSize;
  }

  _sharedAlloc.destroy(texture);
  auto                  imageInfo = nvvk::makeImage2DCreateInfo(extent, format, usage);
  nvvk::Image           image     = _sharedAlloc.createImage(imageInfo);
  VkImageViewCreateInfo ivInfo    = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
  VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  texture                        = _sharedAlloc.createTexture(image, ivInfo, sampler);
  texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}

void RasterPipeline::createFramebuffer()
{
  if(_renderPass == VK_NULL_HANDLE || _offscreenColor.descriptor.imageView == VK_NULL_HANDLE
     || _offscreenObjectId.descriptor.imageView == VK_NULL_HANDLE
     || _offscreenInstanceId.descriptor.imageView == VK_NULL_HANDLE
     || _offscreenDepthAov.descriptor.imageView == VK_NULL_HANDLE
     || _offscreenDepth.descriptor.imageView == VK_NULL_HANDLE)
  {
    return;
  }

  destroyFramebuffer();

  std::array<VkImageView, 5> attachments{_offscreenColor.descriptor.imageView, _offscreenObjectId.descriptor.imageView,
                                         _offscreenInstanceId.descriptor.imageView,
                                         _offscreenDepthAov.descriptor.imageView, _offscreenDepth.descriptor.imageView};

  VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebufferInfo.renderPass      = _renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments    = attachments.data();
  framebufferInfo.width           = _aovSize.width;
  framebufferInfo.height          = _aovSize.height;
  framebufferInfo.layers          = 1;

  if(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffer) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create raster framebuffer");
  }
}

void RasterPipeline::destroyFramebuffer()
{
  if(_device != VK_NULL_HANDLE && _framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(_device, _framebuffer, nullptr);
    _framebuffer = VK_NULL_HANDLE;
  }
}
