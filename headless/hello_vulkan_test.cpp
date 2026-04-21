#include <sstream>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// #define STB_IMAGE_IMPLEMENTATION
// #include <glm/glm.hpp>
// #include "stb_image.h"

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

// 读取 objectId 图像到 CPU 向量 (uint32 per pixel)
std::vector<uint32_t> HelloVulkan::readObjectIdImage()
{
  std::vector<uint32_t> result(m_size.width * m_size.height, 0);

  // 创建 staging buffer
  VkDeviceSize       imageSize = m_size.width * m_size.height * sizeof(uint32_t);
  VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bInfo.size  = imageSize;
  bInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer staging;
  vkCreateBuffer(m_device, &bInfo, nullptr, &staging);

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(m_device, staging, &memReq);

  VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex =
      getMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VkDeviceMemory mem;
  vkAllocateMemory(m_device, &allocInfo, nullptr, &mem);
  vkBindBufferMemory(m_device, staging, mem, 0);

  // 拷贝
  VkCommandBuffer cmd = createTempCmdBuffer();

  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout        = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.image            = m_offscreenObjectId.image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask    = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent      = {m_size.width, m_size.height, 1};

  vkCmdCopyImageToBuffer(cmd, m_offscreenObjectId.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);

  // 还原
  std::swap(barrier.oldLayout, barrier.newLayout);
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  submitTempCmdBuffer(cmd);

  void* mapped = nullptr;
  vkMapMemory(m_device, mem, 0, imageSize, 0, &mapped);
  memcpy(result.data(), mapped, imageSize);
  vkUnmapMemory(m_device, mem);

  vkDestroyBuffer(m_device, staging, nullptr);
  vkFreeMemory(m_device, mem, nullptr);

  return result;
}


//-----------------------------------------------------------------------------------------------------
// for demo local test
//
//--------------------------------------------------------------------------------------------------
// 让Wuson模型实例围绕场景圆形运动
// time: 当前时间（秒），用于动画偏移
void HelloVulkan::animationInstances(float time)
{
  const auto  nbWuson     = static_cast<int32_t>(m_instances.size() - 2);  // 除去plane和sphere的实例数
  const float deltaAngle  = 6.28318530718f / static_cast<float>(nbWuson);  // 平均分配角度
  const float wusonLength = 3.f;
  const float radius      = wusonLength / (2.f * sin(deltaAngle / 2.0f));
  const float offset      = time * 0.5f;  // 时间偏移，实现流畅转动效果

  for(int i = 0; i < nbWuson; i++)
  {
    int       wusonIdx  = i + 1;
    glm::mat4 transform = m_instances[wusonIdx].transform;
    transform           = glm::rotate(transform, i * deltaAngle + offset, glm::vec3(0.f, 1.f, 0.f));
    transform           = glm::translate(transform, glm::vec3(radius, 0.f, 0.f));

    VkAccelerationStructureInstanceKHR& tinst = m_tlas[wusonIdx];
    tinst.transform                           = nvvk::toTransformMatrixKHR(transform);
  }

  // 动画后更新TLAS，使光追实例变换生效
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags, true);
}

//--------------------------------------------------------------------------------------------------
// 用计算着色器动画地修改球体顶点
// 每帧调用，推动球体模型实现变形动画
void HelloVulkan::animationObject(float time)
{
  if(m_compPipeline == VK_NULL_HANDLE || m_compPipelineLayout == VK_NULL_HANDLE || m_compDescSet == VK_NULL_HANDLE)
  {
    return;
  }

  const uint32_t sphereId = 2;
  ObjModel&      model    = m_objModel[sphereId];

  // 更新计算用的描述符集，使其指向球的顶点buffer
  updateCompDescriptors(model.vertexBuffer);

  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  // 绑定计算管线和描述符集，推送当前时间作为push constant
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_compPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_compPipelineLayout, 0, 1, &m_compDescSet, 0, nullptr);
  vkCmdPushConstants(cmdBuf, m_compPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &time);
  vkCmdDispatch(cmdBuf, model.nbVertices, 1, 1);  // 每个顶点一个线程

  genCmdBuf.submitAndWait(cmdBuf);

  // 动画后更新该球的BLAS，使光追结构与顶点位置同步
  m_rtBuilder.updateBlas(sphereId, m_blas[sphereId],
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

//--------------------------------------------------------------------------------------------------
// 创建计算着色器的描述符集布局和描述符集
// 主要用于动画球体顶点（storage buffer）
void HelloVulkan::createCompDescriptors()
{
  m_compDescSetLayoutBind.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);

  m_compDescSetLayout = m_compDescSetLayoutBind.createLayout(m_device);
  m_compDescPool      = m_compDescSetLayoutBind.createPool(m_device, 1);
  m_compDescSet       = nvvk::allocateDescriptorSet(m_device, m_compDescPool, m_compDescSetLayout);
}

//--------------------------------------------------------------------------------------------------
// 更新计算描述符集指向的顶点buffer
// vertex: 球体顶点buffer
void HelloVulkan::updateCompDescriptors(nvvk::Buffer& vertex)
{
  std::vector<VkWriteDescriptorSet> writes;
  VkDescriptorBufferInfo            dbiUnif{vertex.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_compDescSetLayoutBind.makeWrite(m_compDescSet, 0, &dbiUnif));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 创建计算着色器管线
// 包括管线layout（含push constant）、计算管线对象
void HelloVulkan::createCompPipelines()
{
  // push constant: 传递动画时间
  VkPushConstantRange push_constants = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)};

  VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  createInfo.setLayoutCount         = 1;
  createInfo.pSetLayouts            = &m_compDescSetLayout;
  createInfo.pushConstantRangeCount = 1;
  createInfo.pPushConstantRanges    = &push_constants;
  vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_compPipelineLayout);

  VkComputePipelineCreateInfo computePipelineCreateInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  computePipelineCreateInfo.layout = m_compPipelineLayout;

  // 加载编译好的计算着色器SPIR-V
  computePipelineCreateInfo.stage =
      nvvk::createShaderStageInfo(m_device, nvh::loadFile("spv/anim.comp.spv", true, defaultSearchPaths, true),
                                  VK_SHADER_STAGE_COMPUTE_BIT);

  vkCreateComputePipelines(m_device, {}, 1, &computePipelineCreateInfo, nullptr, &m_compPipeline);

  vkDestroyShaderModule(m_device, computePipelineCreateInfo.stage.module, nullptr);
}


void HelloVulkan::dumpInteropTexture(const char* filename)
{
  int width  = m_rtOutputGL.imgSize.width;
  int height = m_rtOutputGL.imgSize.height;
  glBindTexture(GL_TEXTURE_2D, m_rtOutputGL.oglId);
  std::vector<float> pixels(width * height * 4);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());

  std::vector<unsigned char> out_pixels(width * height * 4);
  for(size_t i = 0; i < pixels.size(); ++i)
  {
    float v       = pixels[i];
    v             = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    out_pixels[i] = static_cast<unsigned char>(v * 255.0f);
  }
  stbi_write_png(filename, width, height, 4, out_pixels.data(), width * 4);
  printf("Saved %s (%ux%u)\n", filename, width, height);
}

// 保存最终输出纹理到本地 PNG 文件
void HelloVulkan::saveOffscreenColorToFile(const char* filename)
{
  VkDevice device = m_device;
  VkQueue  queue  = m_queue;

  // 1. 获取 image 信息
  // VkFormat     format    = m_offscreenDenoised.imageFormat;
  VkExtent2D extent    = m_size;
  VkImage    srcImage  = m_offscreenDenoised.image;
  uint32_t   w         = extent.width;
  uint32_t   h         = extent.height;
  size_t     pixelSize = 4 * sizeof(float);  // VK_FORMAT_R32G32B32A32_SFLOAT

  // 2. 创建主机可见buffer
  VkDeviceSize imageSize = w * h * pixelSize;

  VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size        = imageSize;
  bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingMemory;
  vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

  VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocInfo.allocationSize = memReqs.size;
  // 主机可见
  allocInfo.memoryTypeIndex =
      getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
  vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

  // 3. 拷贝 image 到 buffer
  VkCommandBuffer cmd = createTempCmdBuffer();

  // 转换 image layout: GENERAL -> TRANSFER_SRC_OPTIMAL
  VkImageMemoryBarrier imgBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  imgBarrier.oldLayout        = VK_IMAGE_LAYOUT_GENERAL;
  imgBarrier.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  imgBarrier.image            = srcImage;
  imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imgBarrier.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  imgBarrier.dstAccessMask    = VK_ACCESS_TRANSFER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imgBarrier);

  VkBufferImageCopy region               = {};
  region.bufferOffset                    = 0;
  region.bufferRowLength                 = 0;  // tightly packed
  region.bufferImageHeight               = 0;
  region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel       = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount     = 1;
  region.imageOffset                     = {0, 0, 0};
  region.imageExtent                     = {w, h, 1};

  vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

  // 恢复 image layout: TRANSFER_SRC_OPTIMAL -> GENERAL
  std::swap(imgBarrier.oldLayout, imgBarrier.newLayout);
  imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imgBarrier);

  submitTempCmdBuffer(cmd);

  // 4. 映射内存，保存为 PNG
  void* data = nullptr;
  vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);

  // 数据格式: float RGBA，需转为 uint8 RGBA
  std::vector<uint8_t> imageData(w * h * 4);
  float*               src = reinterpret_cast<float*>(data);

  for(uint32_t i = 0; i < w * h; ++i)
  {
    float r              = src[i * 4 + 0];
    float g              = src[i * 4 + 1];
    float b              = src[i * 4 + 2];
    float a              = src[i * 4 + 3];
    imageData[i * 4 + 0] = uint8_t(glm::clamp(r, 0.0f, 1.0f) * 255.0f);
    imageData[i * 4 + 1] = uint8_t(glm::clamp(g, 0.0f, 1.0f) * 255.0f);
    imageData[i * 4 + 2] = uint8_t(glm::clamp(b, 0.0f, 1.0f) * 255.0f);
    imageData[i * 4 + 3] = uint8_t(glm::clamp(a, 0.0f, 1.0f) * 255.0f);
  }

  vkUnmapMemory(device, stagingMemory);

  // 由于 Vulkan 坐标原点左上，PNG 原点左上，通常无需翻转
  stbi_write_png(filename, w, h, 4, imageData.data(), w * 4);

  // 5. 释放资源
  vkFreeMemory(device, stagingMemory, nullptr);
  vkDestroyBuffer(device, stagingBuffer, nullptr);
  printf("Saved %s (%ux%u)\n", filename, w, h);
}
