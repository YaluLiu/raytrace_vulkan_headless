#include "features/tile/tile_atlas_pass.hpp"

#include "features/preview/preview_pipeline.hpp"
#include "scene/gpu_scene.hpp"
#include "scene/scene_descriptors.hpp"
#include "scene/view_uniforms.hpp"
#include "shaders/common/host_device.h"

#include "nvvk/commands_vk.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

void TileAtlasPass::setup(VkDevice device,
                          VkPhysicalDevice physicalDevice,
                          uint32_t graphicsQueueIndex,
                          nvvk::ResourceAllocatorDma& allocator,
                          nvvk::DebugUtil& debug)
{
  m_device = device;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_allocator = &allocator;

  VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  features2.pNext = &features11;
  vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

  VkPhysicalDeviceVulkan11Properties properties11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
  VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  properties2.pNext = &properties11;
  vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

  m_multiviewMaxViewCount = properties11.maxMultiviewViewCount;
  m_multiviewMaxInstanceIndex = properties11.maxMultiviewInstanceIndex;
  m_multiviewSupported =
      features11.multiview == VK_TRUE && m_multiviewMaxViewCount > 0 && MAX_TILE_MULTIVIEW_VIEWS > 0;
  std::cerr << "[Engine] Tile multiview " << (m_multiviewSupported ? "supported" : "not supported")
            << " (maxViewCount=" << m_multiviewMaxViewCount
            << ", maxInstanceIndex=" << m_multiviewMaxInstanceIndex << ")" << std::endl;

  m_colorAtlas.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_depthAtlas.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_multiviewTileTargets.setup(device, graphicsQueueIndex, allocator, debug);
  m_multiviewTilePipeline.setup(device, physicalDevice, graphicsQueueIndex, debug);
}

void TileAtlasPass::destroy()
{
  m_multiviewTileTargets.destroy();
  m_multiviewTilePipeline.destroy();
  m_colorAtlas.destroy();
  m_depthAtlas.destroy();
  m_colorDirty = false;
  m_depthDirty = false;
  m_colorExportValid = false;
  m_depthExportValid = false;
  m_frameId = 0;
  m_lastRenderedDepthCameraCount = 0;
  m_device = VK_NULL_HANDLE;
  m_graphicsQueueIndex = 0;
  m_allocator = nullptr;
}

void TileAtlasPass::destroyGraphicsPipeline()
{
  m_multiviewTilePipeline.destroyGraphicsPipeline();
}

void TileAtlasPass::configure(TileOutputConfig config)
{
  config.atlas.sanitize();
  if(config.atlas != m_config)
  {
    m_config = config.atlas;
    const TileAovChannelMask enabledChannels = m_config.enabledChannels();
    if(!enabledChannels.contains(TileAovChannel::Color))
    {
      m_colorDirty = false;
      m_colorExportValid = false;
    }
    if(!enabledChannels.contains(TileAovChannel::Depth))
    {
      m_depthDirty = false;
      m_depthExportValid = false;
      m_lastRenderedDepthCameraCount = 0;
    }
  }

  m_requestedChannels = config.requestedChannels;
  if(!m_requestedChannels.contains(TileAovChannel::Color))
  {
    m_colorDirty = false;
    m_colorExportValid = false;
  }
  if(!m_requestedChannels.contains(TileAovChannel::Depth))
  {
    m_depthDirty = false;
    m_depthExportValid = false;
    m_lastRenderedDepthCameraCount = 0;
  }
}

void TileAtlasPass::record(const VkCommandBuffer& cmdBuf,
                           const GpuScene& scene,
                           SceneDescriptors& descriptors,
                           ViewUniforms& viewUniforms,
                           const PreviewPipeline& previewPipeline)
{
  ++m_frameId;
  m_colorDirty = false;
  m_depthDirty = false;
  m_colorExportValid = false;
  m_depthExportValid = false;
  m_lastRenderedDepthCameraCount = 0;
  const auto& cameras = viewUniforms.getCameras();
  const TileAovChannelMask channels = effectiveChannels();
  if(channels.none() || cameras.empty())
  {
    return;
  }

  TileAtlasConfig config = m_config;
  config.sanitize();
  const uint32_t capacity = config.capacity();
  if(capacity == 0)
  {
    return;
  }

  const uint32_t cameraCount = static_cast<uint32_t>(std::min<size_t>(cameras.size(), capacity));
  const uint32_t effectiveViewCount = getEffectiveViewCount(cameraCount, capacity);
  m_multiviewEffectiveViewCount = effectiveViewCount;

  auto skip = [&](const char* reason)
  {
    logUnavailableOnce(reason);
  };

  if(effectiveViewCount == 0)
  {
    skip("device unsupported or no usable views");
    return;
  }

  if(viewUniforms.ensureTileFrameUniformBuffer())
  {
    descriptors.updateTileFrameUniform(viewUniforms.getTileFrameUniformDescriptorInfo());
  }

  const VkExtent2D tileExtent = config.tileExtent();
  try
  {
    if(!m_multiviewTilePipeline.ensureResources(descriptors.layout(), previewPipeline, effectiveViewCount))
    {
      skip("failed to create multiview graphics pipeline");
      return;
    }

    if(channels.contains(TileAovChannel::Color) &&
       !m_colorAtlas.ensureResources(config, previewPipeline.getColorFormat(), "TileColorAtlas", true))
    {
      skip("failed to create tile color atlas resources");
      return;
    }

    if(channels.contains(TileAovChannel::Depth) &&
       !m_depthAtlas.ensureResources(config, previewPipeline.getDepthAovFormat(), "TileDepthAtlas", true))
    {
      skip("failed to create tile depth atlas resources");
      return;
    }

    if(!m_multiviewTileTargets.ensureResources(config, previewPipeline, m_multiviewTilePipeline.getRenderPass(),
                                               effectiveViewCount))
    {
      skip("failed to create layered tile resources");
      return;
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "[Engine] Tile multiview setup failed: " << e.what() << std::endl;
    logUnavailableOnce("setup failed");
    return;
  }

  for(uint32_t firstCameraIndex = 0; firstCameraIndex < cameraCount; firstCameraIndex += effectiveViewCount)
  {
    const uint32_t batchCameraCount = std::min(effectiveViewCount, cameraCount - firstCameraIndex);
    viewUniforms.updateTileFrameUniformBufferForBatch(cmdBuf, firstCameraIndex, batchCameraCount, effectiveViewCount,
                                                      tileExtent, scene.getLightCount());
    const bool rendered =
        m_multiviewTilePipeline.draw(cmdBuf, m_multiviewTileTargets.getFramebuffer(), tileExtent,
                                                descriptors.set(), scene.getMeshBuffers(), scene.getInstances(),
                                                scene.getExternalInstanceIds());
    bool copied = rendered;
    if(copied && channels.contains(TileAovChannel::Color))
    {
      copied = m_colorAtlas.copyLayersFrom(cmdBuf, m_multiviewTileTargets.getColorTexture(), firstCameraIndex,
                                           batchCameraCount, m_multiviewTileTargets.getLayerCount());
    }
    if(copied && channels.contains(TileAovChannel::Depth))
    {
      copied = m_depthAtlas.copyLayersFrom(cmdBuf, m_multiviewTileTargets.getDepthAovTexture(), firstCameraIndex,
                                           batchCameraCount, m_multiviewTileTargets.getLayerCount());
    }
    if(!copied)
    {
      skip("batch render or layer copy failed");
      return;
    }
  }

  m_colorDirty = cameraCount > 0 && channels.contains(TileAovChannel::Color);
  m_depthDirty = cameraCount > 0 && channels.contains(TileAovChannel::Depth);
  m_colorExportValid = m_colorDirty;
  m_depthExportValid = m_depthDirty;
  m_lastRenderedDepthCameraCount = m_depthDirty ? cameraCount : 0;
}

std::optional<ExportedAovTexture> TileAtlasPass::getAovTexture(Aov aov) const
{
  switch(aov)
  {
  case Aov::TileColor:
    if(!m_colorExportValid || !m_colorAtlas.hasImage())
    {
      return std::nullopt;
    }
    return m_colorAtlas.getAovTexture();
  case Aov::TileDepth:
    if(!m_depthExportValid || !m_depthAtlas.hasImage())
    {
      return std::nullopt;
    }
    return m_depthAtlas.getAovTexture();
  case Aov::Color:
  case Aov::PrimId:
  case Aov::InstanceId:
  case Aov::Depth:
    return std::nullopt;
  }
  return std::nullopt;
}

TileDepthFrame TileAtlasPass::readDepthFrame()
{
  TileDepthFrame frame;
  frame.frameId = m_frameId;
  frame.cameraCount = m_lastRenderedDepthCameraCount;
  frame.width = m_config.tilePixelWidth;
  frame.height = m_config.tilePixelHeight;

  if(frame.cameraCount == 0 || frame.width == 0 || frame.height == 0 || m_device == VK_NULL_HANDLE ||
     m_allocator == nullptr || !m_depthAtlas.hasImage())
  {
    return frame;
  }

  const uint64_t pixelsPerCamera = static_cast<uint64_t>(frame.width) * frame.height;
  if(pixelsPerCamera == 0 ||
     frame.cameraCount > std::numeric_limits<uint64_t>::max() / pixelsPerCamera)
  {
    frame.cameraCount = 0;
    return frame;
  }
  const uint64_t pixelCount = pixelsPerCamera * frame.cameraCount;
  if(pixelCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
     pixelCount > static_cast<uint64_t>(std::numeric_limits<VkDeviceSize>::max() / sizeof(float)))
  {
    frame.cameraCount = 0;
    return frame;
  }

  const nvvk::Texture& depthTexture = m_depthAtlas.getTexture();
  std::vector<VkBufferImageCopy> copyRegions;
  copyRegions.reserve(frame.cameraCount);
  for(uint32_t cameraIndex = 0; cameraIndex < frame.cameraCount; ++cameraIndex)
  {
    const VkRect2D tileRect = m_depthAtlas.getTileRect(cameraIndex);
    VkBufferImageCopy region{};
    region.bufferOffset = static_cast<VkDeviceSize>(cameraIndex) * pixelsPerCamera * sizeof(float);
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {tileRect.offset.x, tileRect.offset.y, 0};
    region.imageExtent = {frame.width, frame.height, 1};
    copyRegions.push_back(region);
  }
  frame.values.resize(static_cast<size_t>(pixelCount));

  const VkDeviceSize byteCount = static_cast<VkDeviceSize>(pixelCount * sizeof(float));
  nvvk::Buffer stagingBuffer =
      m_allocator->createBuffer(byteCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if(stagingBuffer.buffer == VK_NULL_HANDLE)
  {
    frame.cameraCount = 0;
    frame.values.clear();
    return frame;
  }

  {
    nvvk::CommandPool commandPool(m_device, m_graphicsQueueIndex);
    VkCommandBuffer cmdBuf = commandPool.createCommandBuffer();

    VkImageMemoryBarrier beforeCopy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    beforeCopy.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    beforeCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    beforeCopy.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    beforeCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    beforeCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beforeCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beforeCopy.image = depthTexture.image;
    beforeCopy.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &beforeCopy);

    vkCmdCopyImageToBuffer(cmdBuf, depthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer,
                           static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

    VkImageMemoryBarrier afterCopy = beforeCopy;
    afterCopy.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    afterCopy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    afterCopy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    afterCopy.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &afterCopy);

    commandPool.submitAndWait(cmdBuf);
  }

  const void* mapped = m_allocator->map(stagingBuffer);
  if(mapped == nullptr)
  {
    m_allocator->destroy(stagingBuffer);
    frame.cameraCount = 0;
    frame.values.clear();
    return frame;
  }

  const auto* source = static_cast<const float*>(mapped);
  for(uint32_t cameraIndex = 0; cameraIndex < frame.cameraCount; ++cameraIndex)
  {
    const size_t cameraOffset = static_cast<size_t>(cameraIndex * pixelsPerCamera);
    for(uint32_t y = 0; y < frame.height; ++y)
    {
      const size_t sourceOffset = cameraOffset + static_cast<size_t>(frame.height - 1 - y) * frame.width;
      const size_t destinationOffset = cameraOffset + static_cast<size_t>(y) * frame.width;
      std::memcpy(frame.values.data() + destinationOffset, source + sourceOffset,
                  static_cast<size_t>(frame.width) * sizeof(float));
    }
  }

  m_allocator->unmap(stagingBuffer);
  m_allocator->destroy(stagingBuffer);
  return frame;
}

void TileAtlasPass::markConsumed(const std::string& /*outputDirectory*/)
{
  // Local tile file output is disabled; Hydra consumes tile atlas AOVs directly.
  m_colorDirty = false;
  m_depthDirty = false;
}

uint32_t TileAtlasPass::getEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const
{
  if(!m_multiviewSupported || cameraCount == 0 || capacity == 0)
  {
    return 0;
  }

  const uint32_t deviceCap = std::min<uint32_t>(m_multiviewMaxViewCount, MAX_TILE_MULTIVIEW_VIEWS);
  if(deviceCap == 0)
  {
    return 0;
  }

  return std::min({cameraCount, capacity, deviceCap});
}

TileAovChannelMask TileAtlasPass::effectiveChannels() const
{
  return m_config.enabledChannels() & m_requestedChannels;
}

void TileAtlasPass::logUnavailableOnce(const char* reason)
{
  if(m_multiviewUnsupportedLogged)
  {
    return;
  }

  m_multiviewUnsupportedLogged = true;
  std::cerr << "[Engine] Tile multiview unavailable";
  if(reason != nullptr && reason[0] != '\0')
  {
    std::cerr << ": " << reason;
  }
  std::cerr << std::endl;
}
