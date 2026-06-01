#include "features/tile/tile_atlas_pass.hpp"

#include "features/preview/preview_pipeline.hpp"
#include "scene/gpu_scene.hpp"
#include "scene/scene_descriptors.hpp"
#include "scene/view_uniforms.hpp"
#include "shaders/common/host_device.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

void TileAtlasPass::setup(VkDevice device,
                          VkPhysicalDevice physicalDevice,
                          uint32_t graphicsQueueIndex,
                          nvvk::ResourceAllocatorDma& allocator,
                          nvvk::DebugUtil& debug)
{
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
  std::cerr << "[Renderer] Tile multiview " << (m_multiviewSupported ? "supported" : "not supported")
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
}

void TileAtlasPass::destroyGraphicsPipeline()
{
  m_multiviewTilePipeline.destroyGraphicsPipeline();
}

void TileAtlasPass::setConfig(TileAtlasConfig config)
{
  config.sanitize();
  if(config != m_config)
  {
    m_config = config;
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
    }
  }
}

void TileAtlasPass::setRequestedChannels(TileAovChannelMask channels)
{
  m_requestedChannels = channels;
  if(!m_requestedChannels.contains(TileAovChannel::Color))
  {
    m_colorDirty = false;
    m_colorExportValid = false;
  }
  if(!m_requestedChannels.contains(TileAovChannel::Depth))
  {
    m_depthDirty = false;
    m_depthExportValid = false;
  }
}

void TileAtlasPass::record(const VkCommandBuffer& cmdBuf,
                           const GpuScene& scene,
                           SceneDescriptors& descriptors,
                           ViewUniforms& viewUniforms,
                           const PreviewPipeline& previewPipeline)
{
  m_colorDirty = false;
  m_depthDirty = false;
  m_colorExportValid = false;
  m_depthExportValid = false;
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
    std::cerr << "[Renderer] Tile multiview setup failed: " << e.what() << std::endl;
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
                                                scene.getInstanceIds());
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
  std::cerr << "[Renderer] Tile multiview unavailable";
  if(reason != nullptr && reason[0] != '\0')
  {
    std::cerr << ": " << reason;
  }
  std::cerr << std::endl;
}
