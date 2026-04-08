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

}  // namespace

//--------------------------------------------------------------------------------------------------
// update material
//--------------------------------------------------------------------------------------------------
void HelloVulkan::updateMaterialAtRuntime(int modelIndex, int materialIndex, const WaveFrontMaterial& newMaterial)
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = cmdGen.createCommandBuffer();

  // 计算偏移量
  VkDeviceSize offset = materialIndex * sizeof(WaveFrontMaterial);

  // 确保缓冲区可见性
  VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.buffer        = m_objModel[modelIndex].matColorBuffer.buffer;
  barrier.offset        = offset;
  barrier.size          = sizeof(WaveFrontMaterial);

  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

  // 更新材质数据
  vkCmdUpdateBuffer(cmdBuf, m_objModel[modelIndex].matColorBuffer.buffer, offset, sizeof(WaveFrontMaterial), &newMaterial);

  // 确保更新后的数据对着色器可见
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0,
                       nullptr, 1, &barrier, 0, nullptr);

  cmdGen.submitAndWait(cmdBuf);
  resetAccumulation();
}

void HelloVulkan::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  // 用一个命令池/命令缓冲，减少同步消耗
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = cmdGen.createCommandBuffer();

  std::vector<VkBufferMemoryBarrier> preBarriers;
  std::vector<VkBufferMemoryBarrier> postBarriers;

  // 1. 先加所有Barrier，保证并行性
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
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, static_cast<uint32_t>(preBarriers.size()),
                       preBarriers.data(), 0, nullptr);

  // 2. 更新所有材质
  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(WaveFrontMaterial);
    vkCmdUpdateBuffer(cmdBuf, m_objModel[upd.modelIndex].matColorBuffer.buffer, offset, sizeof(WaveFrontMaterial), &upd.newMaterial);
  }

  // 3. 再加所有postBarrier，保证对shader可见
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
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0,
                       nullptr, static_cast<uint32_t>(postBarriers.size()), postBarriers.data(), 0, nullptr);

  // 4. 提交命令
  cmdGen.submitAndWait(cmdBuf);
  resetAccumulation();
}

void HelloVulkan::createOutputImage()
{
  m_rtOutputGL.destroy(m_allocGL);

  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenColorFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                                      | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenColor                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

  m_rtOutputGL.imgSize = m_size;
  m_rtOutputGL.texVk   = m_offscreenColor;
  createTextureGL(m_rtOutputGL, GL_RGBA32F, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createDenoisedImage()
{
  m_alloc.destroy(m_offscreenDenoised);
  auto imageInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenDenoisedFormat,
                                                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  nvvk::Image image = m_alloc.createImage(imageInfo);

  VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
  VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  m_offscreenDenoised                        = m_alloc.createTexture(image, ivInfo, sampler);
  m_offscreenDenoised.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}

void HelloVulkan::createObjectIdImage()
{
  m_rtObjectIdGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenObjectIdFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenObjectId                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenObjectId.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }
  m_rtObjectIdGL.imgSize = m_size;
  m_rtObjectIdGL.texVk   = m_offscreenObjectId;
  createTextureGL(m_rtObjectIdGL, GL_R32I, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createInstanceIdImage()
{
  m_rtInstanceIdGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenInstanceIdFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenInstanceId                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenInstanceId.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }
  m_rtInstanceIdGL.imgSize = m_size;
  m_rtInstanceIdGL.texVk   = m_offscreenInstanceId;
  createTextureGL(m_rtInstanceIdGL, GL_R32I, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createDiffuseAlbedoImage()
{
  m_rtDiffuseAlbedoGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenDiffuseAlbedoFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenDiffuseAlbedo                        = m_allocGL.createTexture(image, ivInfo, sampler);
  }
  m_offscreenDiffuseAlbedo.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_rtDiffuseAlbedoGL.imgSize = m_size;
  m_rtDiffuseAlbedoGL.texVk   = m_offscreenDiffuseAlbedo;
  createTextureGL(m_rtDiffuseAlbedoGL, GL_RGBA32F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createSpecularAlbedoImage()
{
  m_rtSpecularAlbedoGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenSpecularAlbedoFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenSpecularAlbedo                        = m_allocGL.createTexture(image, ivInfo, sampler);
  }
  m_offscreenSpecularAlbedo.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_rtSpecularAlbedoGL.imgSize = m_size;
  m_rtSpecularAlbedoGL.texVk   = m_offscreenSpecularAlbedo;
  createTextureGL(m_rtSpecularAlbedoGL, GL_RGBA32F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createNormalRoughnessImage()
{
  m_rtNormalRoughnessGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenNormalRoughnessFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenNormalRoughness                        = m_allocGL.createTexture(image, ivInfo, sampler);
  }
  m_offscreenNormalRoughness.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_rtNormalRoughnessGL.imgSize = m_size;
  m_rtNormalRoughnessGL.texVk   = m_offscreenNormalRoughness;
  createTextureGL(m_rtNormalRoughnessGL, GL_RGBA32F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createMotionVectorImage()
{
  m_rtMotionVectorGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenMotionVectorFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenMotionVector                        = m_allocGL.createTexture(image, ivInfo, sampler);
  }
  m_offscreenMotionVector.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_rtMotionVectorGL.imgSize = m_size;
  m_rtMotionVectorGL.texVk   = m_offscreenMotionVector;
  createTextureGL(m_rtMotionVectorGL, GL_RG32F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createLinearDepthImage()
{
  m_rtLinearDepthGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenLinearDepthFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenLinearDepth                        = m_allocGL.createTexture(image, ivInfo, sampler);
  }
  m_offscreenLinearDepth.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_rtLinearDepthGL.imgSize = m_size;
  m_rtLinearDepthGL.texVk   = m_offscreenLinearDepth;
  createTextureGL(m_rtLinearDepthGL, GL_R32F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

void HelloVulkan::createSpecularHitDistanceImage()
{
  m_rtSpecularHitDistanceGL.destroy(m_allocGL);
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenSpecularHitDistanceFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenSpecularHitDistance                        = m_allocGL.createTexture(image, ivInfo, sampler);
  }
  m_offscreenSpecularHitDistance.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_rtSpecularHitDistanceGL.imgSize = m_size;
  m_rtSpecularHitDistanceGL.texVk   = m_offscreenSpecularHitDistance;
  createTextureGL(m_rtSpecularHitDistanceGL, GL_R32F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
}

//--------------------------------------------------------------------------------------------------
// 灯光管理函数
//
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
  // 创建灯光缓冲区（初始支持100个灯光）
  size_t maxLights  = 100;
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
    resetAccumulation();
    m_uploadedLights = m_lights;
  }

  if(!m_lights.empty())
  {
    vkCmdUpdateBuffer(cmdBuf, m_bLights.buffer, 0, sizeof(Light) * m_lights.size(), m_lights.data());
  }
}

void HelloVulkan::createInstanceIdBuffer()
{
  size_t maxInstances = std::max(size_t(1000), m_instanceIds.size() * 2);  // 预留空间
  size_t bufferSize   = sizeof(int) * maxInstances;

  m_bInstanceIds = m_alloc.createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug.setObjectName(m_bInstanceIds.buffer, "InstanceIds");
}

void HelloVulkan::updateInstanceIdBuffer(const VkCommandBuffer& cmdBuf)
{
  if(!m_instanceIds.empty())
  {
    vkCmdUpdateBuffer(cmdBuf, m_bInstanceIds.buffer, 0, sizeof(int) * m_instanceIds.size(), m_instanceIds.data());
  }
}
