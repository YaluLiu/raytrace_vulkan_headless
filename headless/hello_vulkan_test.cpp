#include <sstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "hello_vulkan.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/buffers_vk.hpp"

std::vector<uint32_t> HelloVulkan::readObjectIdImage()
{
  const VkExtent2D    aovSize         = m_mainRasterPipeline.getAovSize();
  const nvvk::Texture& objectIdTexture = m_mainRasterPipeline.getObjectIdTextureForReadback();
  std::vector<uint32_t> result(aovSize.width * aovSize.height, 0);

  VkDeviceSize       imageSize = aovSize.width * aovSize.height * sizeof(uint32_t);
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

  VkCommandBuffer cmd = createTempCmdBuffer();

  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout        = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.image            = objectIdTexture.image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask    = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent      = {aovSize.width, aovSize.height, 1};

  vkCmdCopyImageToBuffer(cmd, objectIdTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);

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

void HelloVulkan::saveOffscreenColorToFile(const char* filename)
{
  VkDevice device = m_device;
  VkQueue  queue  = m_queue;

  const VkExtent2D     extent       = m_mainRasterPipeline.getRenderSize();
  const nvvk::Texture& colorTexture = m_mainRasterPipeline.getColorTextureForReadback();
  VkImage              srcImage     = colorTexture.image;
  uint32_t   w         = extent.width;
  uint32_t   h         = extent.height;
  size_t     pixelSize = 4 * sizeof(float);  // VK_FORMAT_R32G32B32A32_SFLOAT

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
  allocInfo.memoryTypeIndex =
      getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
  vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

  VkCommandBuffer cmd = createTempCmdBuffer();

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

  std::swap(imgBarrier.oldLayout, imgBarrier.newLayout);
  imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imgBarrier);

  submitTempCmdBuffer(cmd);

  void* data = nullptr;
  vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);

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

  stbi_write_png(filename, w, h, 4, imageData.data(), w * 4);

  vkFreeMemory(device, stagingMemory, nullptr);
  vkDestroyBuffer(device, stagingBuffer, nullptr);
}
