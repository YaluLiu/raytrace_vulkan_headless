#include "hello_vulkan.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/common.hpp>

#include "hello_vulkan_barriers.hpp"
#include "stb_image_write.h"

namespace
{
std::string joinPath(const std::filesystem::path &directory, const char *filename)
{
  return (directory / filename).lexically_normal().string();
}
} // namespace

void HelloVulkan::renderTileAtlas(const VkCommandBuffer &cmdBuf)
{
  m_tileAtlasExportValid = false;
  if(!m_tileConfig.enabled || m_cameras.empty())
  {
    return;
  }

  HeadlessTileConfig config = m_tileConfig;
  config.sanitize();
  const uint32_t capacity = config.capacity();
  if(capacity == 0)
  {
    return;
  }

  const VkExtent2D tileExtent = config.tileExtent();
  if(!m_tileAtlas.ensureResources(config, m_tileRasterPipeline))
  {
    return;
  }

  const uint32_t cameraCount = static_cast<uint32_t>(std::min<size_t>(m_cameras.size(), capacity));
  for(uint32_t cameraIndex = 0; cameraIndex < cameraCount; ++cameraIndex)
  {
    const uint32_t frameUniformSlot = cameraIndex + 1;
    updateUniformBufferForCamera(cmdBuf, m_cameras[cameraIndex], tileExtent, frameUniformSlot);
    const VkRect2D tileRect = m_tileAtlas.getTileRect(cameraIndex);
    VkViewport viewport{static_cast<float>(tileRect.offset.x),
                        static_cast<float>(tileRect.offset.y),
                        static_cast<float>(tileRect.extent.width),
                        static_cast<float>(tileRect.extent.height),
                        0.0f,
                        1.0f};
    m_tileRasterPipeline.rasterizeToFramebuffer(cmdBuf, m_tileAtlas.getFramebuffer(), m_tileAtlas.getExtent(), tileRect,
                                                viewport, tileRect, m_descSet, m_objModel, m_instances, m_instanceIds,
                                                getFrameUniformOffset(frameUniformSlot));
  }

  m_tileAtlasDirty = cameraCount > 0;
  m_tileAtlasExportValid = cameraCount > 0;
}

void HelloVulkan::renderTileAtlasMultiview(const VkCommandBuffer &cmdBuf)
{
  if(m_tileRenderMode == HeadlessTileRenderMode::Serial)
  {
    renderTileAtlas(cmdBuf);
    return;
  }

  m_tileAtlasExportValid = false;
  if(!m_tileConfig.enabled || m_cameras.empty())
  {
    return;
  }

  HeadlessTileConfig config = m_tileConfig;
  config.sanitize();
  const uint32_t capacity = config.capacity();
  if(capacity == 0)
  {
    return;
  }

  const uint32_t cameraCount = static_cast<uint32_t>(std::min<size_t>(m_cameras.size(), capacity));
  const uint32_t effectiveViewCount = getTileMultiviewEffectiveViewCount(cameraCount, capacity);
  m_tileMultiviewEffectiveViewCount = effectiveViewCount;

  auto fallbackOrSkip = [&](const char *reason)
  {
    logTileMultiviewUnavailableOnce(reason);
    if(m_tileRenderMode == HeadlessTileRenderMode::MultiviewPreferred)
    {
      renderTileAtlas(cmdBuf);
    }
  };

  if(effectiveViewCount == 0)
  {
    fallbackOrSkip("device unsupported or no usable views");
    return;
  }

  ensureTileFrameUniformBuffer();
  updateTileFrameUniformDescriptor();

  const VkExtent2D tileExtent = config.tileExtent();
  try
  {
    if(!m_tileMultiviewRasterPipeline.ensureResources(m_descSetLayout, m_tileRasterPipeline, effectiveViewCount))
    {
      fallbackOrSkip("failed to create multiview raster pipeline");
      return;
    }

    if(!m_tileAtlas.ensureResources(config, m_tileRasterPipeline))
    {
      fallbackOrSkip("failed to create tile atlas resources");
      return;
    }

    if(!m_tileLayerOutput.ensureResources(config, m_tileRasterPipeline, m_tileMultiviewRasterPipeline.getRenderPass(),
                                          effectiveViewCount))
    {
      fallbackOrSkip("failed to create layered tile resources");
      return;
    }
  }
  catch(const std::exception &e)
  {
    std::cerr << "[HelloVulkan] Tile multiview setup failed: " << e.what() << std::endl;
    if(m_tileRenderMode == HeadlessTileRenderMode::MultiviewPreferred)
    {
      renderTileAtlas(cmdBuf);
    }
    return;
  }

  for(uint32_t firstCameraIndex = 0; firstCameraIndex < cameraCount; firstCameraIndex += effectiveViewCount)
  {
    const uint32_t batchCameraCount = std::min(effectiveViewCount, cameraCount - firstCameraIndex);
    updateTileFrameUniformBufferForBatch(cmdBuf, firstCameraIndex, batchCameraCount, effectiveViewCount, tileExtent);
    const bool rendered = m_tileMultiviewRasterPipeline.rasterize(
        cmdBuf, m_tileLayerOutput.getFramebuffer(), tileExtent, m_descSet, m_objModel, m_instances, m_instanceIds);
    const bool copied =
        rendered && m_tileAtlas.copyLayersFrom(cmdBuf, m_tileLayerOutput, firstCameraIndex, batchCameraCount);
    if(!copied)
    {
      fallbackOrSkip("batch render or layer copy failed");
      return;
    }
  }

  m_tileAtlasDirty = cameraCount > 0;
  m_tileAtlasExportValid = cameraCount > 0;
}

void HelloVulkan::saveTileAtlasOutputs(const std::string &outputDirectory)
{
  if(!m_tileAtlasDirty || !m_tileAtlas.hasImages())
  {
    return;
  }

  const std::filesystem::path outputDir(outputDirectory);
  std::filesystem::create_directories(outputDir);

  const VkExtent2D extent = m_tileAtlas.getExtent();
  const uint32_t pixelCount = extent.width * extent.height;

  auto readFloatImage = [&](const nvvk::Texture &texture, uint32_t componentCount)
  {
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(pixelCount) * componentCount * sizeof(float);
    std::vector<float> result(static_cast<size_t>(pixelCount) * componentCount, 0.0f);

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    if(vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create tile atlas readback buffer");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if(vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS)
    {
      vkDestroyBuffer(m_device, stagingBuffer, nullptr);
      throw std::runtime_error("Failed to allocate tile atlas readback memory");
    }
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

    VkCommandBuffer cmd = createTempCmdBuffer();

    VkImageMemoryBarrier toTransfer = makeColorImageBarrier(
        texture.image, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &toTransfer);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(cmd, texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

    VkImageMemoryBarrier toGeneral =
        makeColorImageBarrier(texture.image, VK_ACCESS_TRANSFER_READ_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &toGeneral);

    submitTempCmdBuffer(cmd);

    void *mapped = nullptr;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &mapped);
    std::memcpy(result.data(), mapped, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device, stagingMemory);

    vkFreeMemory(m_device, stagingMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    return result;
  };

  const std::vector<float> colorFloats = readFloatImage(m_tileAtlas.getColorTexture(), 4);
  std::vector<uint8_t> colorBytes(static_cast<size_t>(pixelCount) * 4, 255);
  for(uint32_t pixel = 0; pixel < pixelCount; ++pixel)
  {
    colorBytes[pixel * 4 + 0] = static_cast<uint8_t>(glm::clamp(colorFloats[pixel * 4 + 0], 0.0f, 1.0f) * 255.0f);
    colorBytes[pixel * 4 + 1] = static_cast<uint8_t>(glm::clamp(colorFloats[pixel * 4 + 1], 0.0f, 1.0f) * 255.0f);
    colorBytes[pixel * 4 + 2] = static_cast<uint8_t>(glm::clamp(colorFloats[pixel * 4 + 2], 0.0f, 1.0f) * 255.0f);
    colorBytes[pixel * 4 + 3] = static_cast<uint8_t>(glm::clamp(colorFloats[pixel * 4 + 3], 0.0f, 1.0f) * 255.0f);
  }

  const std::string colorPath = joinPath(outputDir, "tile_color_atlas.png");
  if(stbi_write_png(colorPath.c_str(), static_cast<int>(extent.width), static_cast<int>(extent.height), 4,
                    colorBytes.data(), static_cast<int>(extent.width * 4)) == 0)
  {
    throw std::runtime_error("Failed to write tile color atlas: " + colorPath);
  }

  const std::vector<float> depthFloats = readFloatImage(m_tileAtlas.getDepthTexture(), 1);
  const std::string depthPath = joinPath(outputDir, "tile_depth_atlas.f32");
  std::ofstream depthFile(depthPath, std::ios::binary | std::ios::trunc);
  if(!depthFile)
  {
    throw std::runtime_error("Failed to open tile depth atlas: " + depthPath);
  }
  depthFile.write(reinterpret_cast<const char *>(depthFloats.data()),
                  static_cast<std::streamsize>(depthFloats.size() * sizeof(float)));
  if(!depthFile)
  {
    throw std::runtime_error("Failed to write tile depth atlas: " + depthPath);
  }

  const HeadlessTileConfig &config = m_tileAtlas.getConfig();
  const std::string metadataPath = joinPath(outputDir, "tile_depth_atlas.json");
  std::ofstream metadata(metadataPath, std::ios::trunc);
  if(!metadata)
  {
    throw std::runtime_error("Failed to open tile depth metadata: " + metadataPath);
  }
  metadata << "{\n"
           << "  \"width\": " << extent.width << ",\n"
           << "  \"height\": " << extent.height << ",\n"
           << "  \"format\": \"float32\",\n"
           << "  \"layout\": \"row-major tiles\",\n"
           << "  \"tileCameraWidth\": " << config.cameraWidth << ",\n"
           << "  \"tileCameraHeight\": " << config.cameraHeight << ",\n"
           << "  \"gridColumns\": " << config.gridColumns << ",\n"
           << "  \"gridRows\": " << config.gridRows << "\n"
           << "}\n";

  m_tileAtlasDirty = false;
}
