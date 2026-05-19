#include <sstream>

#include "raster_renderer.hpp"
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
#include <algorithm>

void RasterRenderer::updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate>& updates)
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = cmdGen.createCommandBuffer();

  std::vector<VkBufferMemoryBarrier> preBarriers;
  std::vector<VkBufferMemoryBarrier> postBarriers;

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(WaveFrontMaterial);

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer        = m_objModel[upd.modelIndex].matColorBuffer.buffer;
    preBarrier.offset        = offset;
    preBarrier.size          = sizeof(WaveFrontMaterial);

    preBarriers.push_back(preBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(preBarriers.size()), preBarriers.data(), 0, nullptr);

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(WaveFrontMaterial);
    vkCmdUpdateBuffer(cmdBuf, m_objModel[upd.modelIndex].matColorBuffer.buffer, offset, sizeof(WaveFrontMaterial), &upd.newMaterial);
  }

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(WaveFrontMaterial);

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer        = m_objModel[upd.modelIndex].matColorBuffer.buffer;
    postBarrier.offset        = offset;
    postBarrier.size          = sizeof(WaveFrontMaterial);

    postBarriers.push_back(postBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(postBarriers.size()), postBarriers.data(), 0, nullptr);

  cmdGen.submitAndWait(cmdBuf);
}

void RasterRenderer::addLight(const Light& light)
{
  m_lights.push_back(light);
}

void RasterRenderer::clearLights()
{
  m_lights.clear();
}

void RasterRenderer::createLightBuffer()
{
  size_t maxLights  = MAX_SCENE_LIGHTS;
  size_t bufferSize = sizeof(Light) * maxLights;

  m_bLights = m_alloc.createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug.setObjectName(m_bLights.buffer, "Lights");
}

void RasterRenderer::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  const size_t lightCount = std::min<size_t>(m_lights.size(), MAX_SCENE_LIGHTS);
  if(lightCount > 0)
  {
    const VkDeviceSize lightBufferSize = sizeof(Light) * lightCount;

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer        = m_bLights.buffer;
    preBarrier.offset        = 0;
    preBarrier.size          = lightBufferSize;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &preBarrier, 0, nullptr);

    vkCmdUpdateBuffer(cmdBuf, m_bLights.buffer, 0, lightBufferSize, m_lights.data());

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer        = m_bLights.buffer;
    postBarrier.offset        = 0;
    postBarrier.size          = lightBufferSize;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 1, &postBarrier, 0, nullptr);
  }
}
