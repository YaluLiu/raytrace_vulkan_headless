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


extern std::vector<std::string> defaultSearchPaths;

//--------------------------------------------------------------------------------------------------
// update blas & tlas
//--------------------------------------------------------------------------------------------------
void HelloVulkan::updateTlas(uint32_t mesh_Id, glm::mat4 transform, bool visible)
{
  VkAccelerationStructureInstanceKHR& tinst = m_tlas[mesh_Id];
  tinst.mask                                = visible ? 0xFF : 0x00;
  tinst.transform                           = nvvk::toTransformMatrixKHR(transform);
}

void HelloVulkan::updateTlasEnd()
{
  // Updating the top level acceleration structure
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags, true);
}

// 动画处理球体对象的顶点，在 C++ 端进行缩放
void HelloVulkan::updateBlas(uint32_t mesh_Id)
{
  //更新了loader的对应顶点数据
  std::vector<VertexObj>& now_vertices = m_Loader[mesh_Id].m_vertices;
  ObjModel&               model        = m_objModel[mesh_Id];

  // 创建命令池和命令缓冲区
  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  // 更新模型的顶点数量
  model.nbVertices = static_cast<uint32_t>(now_vertices.size());


  // 定义缓冲区使用标志
  VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  // 销毁旧的顶点缓冲区，防止内存泄漏
  m_alloc.destroy(model.vertexBuffer);

  // 创建新的顶点缓冲区并上传修改后的顶点数据
  model.vertexBuffer = m_alloc.createBuffer(cmdBuf, now_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);

  // 提交命令缓冲区并等待执行完成
  genCmdBuf.submitAndWait(cmdBuf);

  // 更新底层加速结构（BLAS），使用新的顶点缓冲区
  m_rtBuilder.updateBlas(mesh_Id, m_blas[mesh_Id],
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

//--------------------------------------------------------------------------------------------------
// update material
//--------------------------------------------------------------------------------------------------
// 在渲染循环中修改材质
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
}

void HelloVulkan::createOutputImage()
{
#if ENABLE_GL_VK_CONVERSION
  m_rtOutputGL.destroy(m_allocGL);
#else
  m_allocGL.destroy(m_offscreenColor);
#endif

  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenColorFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenColor                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

#if ENABLE_GL_VK_CONVERSION
  m_rtOutputGL.imgSize = m_size;
  m_rtOutputGL.texVk   = m_offscreenColor;
  createTextureGL(m_rtOutputGL, GL_RGBA32F, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, m_allocGL);
#endif
}

void HelloVulkan::createObjectIdImage()
{
#if ENABLE_GL_VK_CONVERSION
  m_rtObjectIdGL.destroy(m_allocGL);
#else
  m_allocGL.destroy(m_offscreenObjectId);
#endif
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenObjectIdFormat,
                                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenObjectId                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenObjectId.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }
#if ENABLE_GL_VK_CONVERSION
  m_rtObjectIdGL.imgSize   = m_size;
  m_rtObjectIdGL.texVk     = m_offscreenObjectId;
  m_rtObjectIdGL.mipLevels = 1;
  createTextureGL(m_rtObjectIdGL, GL_R32I, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
#endif
}

void HelloVulkan::submitFrame()
{
  const VkCommandBuffer& cmdBuf = m_commandBuffers[m_imageIndex];

  VkSubmitInfo         submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
  submitInfo.pWaitDstStageMask    = &stageFlags;
  submitInfo.waitSemaphoreCount   = 1;
  submitInfo.pWaitSemaphores      = &m_semaphores.vkComplete;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &m_semaphores.vkReady;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &cmdBuf;

  vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);

  // 等待队列完成（保证输出image可读取）
  vkQueueWaitIdle(m_queue);
}

void HelloVulkan::createSemaphores()
{
  int glReadyHandle{-1};
  int glCompleteHandle{-1};

  // Vulkan
  {
    auto handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    {
      VkSemaphoreCreateInfo       sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      VkExportSemaphoreCreateInfo esci{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
      sci.pNext        = &esci;
      esci.handleTypes = handleType;
      vkCreateSemaphore(m_device, &sci, nullptr, &m_semaphores.vkReady);
      vkCreateSemaphore(m_device, &sci, nullptr, &m_semaphores.vkComplete);
    }
    {
      VkSemaphoreGetFdInfoKHR getFdInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
      getFdInfo.handleType = handleType;
      getFdInfo.semaphore  = m_semaphores.vkReady;
      vkGetSemaphoreFdKHR(m_device, &getFdInfo, &glReadyHandle);
    }
    {
      VkSemaphoreGetFdInfoKHR getFdInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
      getFdInfo.handleType = handleType;
      getFdInfo.semaphore  = m_semaphores.vkComplete;
      vkGetSemaphoreFdKHR(m_device, &getFdInfo, &glCompleteHandle);
    }
  }

  // OpenGL
  {
    // Import semaphores
    glGenSemaphoresEXT(1, &m_semaphores.glReady);
    glGenSemaphoresEXT(1, &m_semaphores.glComplete);
    glImportSemaphoreFdEXT(m_semaphores.glReady, GL_HANDLE_TYPE_OPAQUE_FD_EXT, glReadyHandle);
    glImportSemaphoreFdEXT(m_semaphores.glComplete, GL_HANDLE_TYPE_OPAQUE_FD_EXT, glCompleteHandle);
  }
}