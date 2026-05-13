#include <sstream>

#include "hello_vulkan.hpp"
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
#include <cmath>

namespace {
bool nearlyEqual(float a, float b, float eps = 1e-5f)
{
  return std::fabs(a - b) <= eps;
}

bool lightEqual(const Light& lhs, const Light& rhs)
{
  return lhs.type == rhs.type && lhs.textureID == rhs.textureID && nearlyEqual(lhs.baseEmission.x, rhs.baseEmission.x)
         && nearlyEqual(lhs.baseEmission.y, rhs.baseEmission.y) && nearlyEqual(lhs.baseEmission.z, rhs.baseEmission.z)
         && nearlyEqual(lhs.diffuse, rhs.diffuse) && nearlyEqual(lhs.specular, rhs.specular)
         && nearlyEqual(lhs.direction.x, rhs.direction.x) && nearlyEqual(lhs.direction.y, rhs.direction.y)
         && nearlyEqual(lhs.direction.z, rhs.direction.z) && nearlyEqual(lhs.angle, rhs.angle)
         && nearlyEqual(lhs.position.x, rhs.position.x) && nearlyEqual(lhs.position.y, rhs.position.y)
         && nearlyEqual(lhs.position.z, rhs.position.z) && nearlyEqual(lhs.radius, rhs.radius)
         && nearlyEqual(lhs.rotateQuat.x, rhs.rotateQuat.x) && nearlyEqual(lhs.rotateQuat.y, rhs.rotateQuat.y)
         && nearlyEqual(lhs.rotateQuat.z, rhs.rotateQuat.z) && nearlyEqual(lhs.rotateQuat.w, rhs.rotateQuat.w);
}

}

void HelloVulkan::updateMaterialAtRuntime(int modelIndex, int materialIndex, const WaveFrontMaterial& newMaterial)
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = cmdGen.createCommandBuffer();

  VkDeviceSize offset = materialIndex * sizeof(WaveFrontMaterial);

  VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.buffer        = m_objModel[modelIndex].matColorBuffer.buffer;
  barrier.offset        = offset;
  barrier.size          = sizeof(WaveFrontMaterial);

  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                       &barrier, 0, nullptr);

  vkCmdUpdateBuffer(cmdBuf, m_objModel[modelIndex].matColorBuffer.buffer, offset, sizeof(WaveFrontMaterial), &newMaterial);

  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1,
                       &barrier, 0, nullptr);

  cmdGen.submitAndWait(cmdBuf);
  resetFrameHistory();
}

void HelloVulkan::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
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
  resetFrameHistory();
}

void HelloVulkan::createOffscreenImage(nvvk::Texture& texture,
                                       VkFormat format,
                                       VkImageUsageFlags usage,
                                       VkExtent2D extent)
{
  if(extent.width == 0 || extent.height == 0)
  {
    extent = m_size;
  }

  m_sharedAlloc.destroy(texture);
  auto                  imageInfo = nvvk::makeImage2DCreateInfo(extent, format, usage);
  nvvk::Image           image     = m_sharedAlloc.createImage(imageInfo);
  VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
  VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  texture                        = m_sharedAlloc.createTexture(image, ivInfo, sampler);
  texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}

void HelloVulkan::addLight(const Light& light)
{
  m_lights.push_back(light);
}

void HelloVulkan::clearLights()
{
  m_lights.clear();
}

void HelloVulkan::createLightBuffer()
{
  size_t maxLights  = MAX_SCENE_LIGHTS;
  size_t bufferSize = sizeof(Light) * maxLights;

  m_bLights = m_alloc.createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug.setObjectName(m_bLights.buffer, "Lights");
}

void HelloVulkan::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  bool changed = m_lights.size() != m_uploadedLights.size();
  if(!changed)
  {
    for(size_t i = 0; i < m_lights.size(); ++i)
    {
      if(!lightEqual(m_lights[i], m_uploadedLights[i]))
      {
        changed = true;
        break;
      }
    }
  }

  if(changed)
  {
    resetFrameHistory();
    m_uploadedLights = m_lights;
  }

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
