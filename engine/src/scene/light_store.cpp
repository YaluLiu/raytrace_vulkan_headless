#include "scene/light_store.hpp"

#include <algorithm>

void LightStore::setup(nvvk::ResourceAllocatorDma& allocator, nvvk::DebugUtil& debug)
{
  m_alloc = &allocator;
  m_debug = &debug;
}

void LightStore::destroy()
{
  if(m_alloc != nullptr)
  {
    m_alloc->destroy(m_buffer);
  }
  m_buffer = {};
  m_lights.clear();
}

void LightStore::addLight(const Light& light)
{
  m_lights.push_back(light);
}

void LightStore::clearLights()
{
  m_lights.clear();
}

void LightStore::createLightBuffer()
{
  size_t maxLights = MAX_SCENE_LIGHTS;
  size_t bufferSize = sizeof(Light) * maxLights;

  m_alloc->destroy(m_buffer);
  m_buffer = m_alloc->createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug->setObjectName(m_buffer.buffer, "Lights");
}

void LightStore::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  const size_t lightCount = std::min<size_t>(m_lights.size(), MAX_SCENE_LIGHTS);
  if(lightCount > 0)
  {
    const VkDeviceSize lightBufferSize = sizeof(Light) * lightCount;

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer = m_buffer.buffer;
    preBarrier.offset = 0;
    preBarrier.size = lightBufferSize;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         1, &preBarrier, 0, nullptr);

    vkCmdUpdateBuffer(cmdBuf, m_buffer.buffer, 0, lightBufferSize, m_lights.data());

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer = m_buffer.buffer;
    postBarrier.offset = 0;
    postBarrier.size = lightBufferSize;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         1, &postBarrier, 0, nullptr);
  }
}
