#include "tile_atlas_pass.hpp"

#include "preview_raster_pipeline.hpp"
#include "raster_gpu_scene.hpp"
#include "raster_scene_descriptors.hpp"
#include "raster_view_uniforms.hpp"
#include "shaders/host_device.h"

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
  std::cerr << "[RasterRenderer] Tile multiview " << (m_multiviewSupported ? "supported" : "not supported")
            << " (maxViewCount=" << m_multiviewMaxViewCount
            << ", maxInstanceIndex=" << m_multiviewMaxInstanceIndex << ")" << std::endl;

  m_tileAovAtlas.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_multiviewTileTargets.setup(device, graphicsQueueIndex, allocator, debug);
  m_multiviewTileRasterPipeline.setup(device, physicalDevice, graphicsQueueIndex, debug);
}

void TileAtlasPass::destroy()
{
  m_multiviewTileTargets.destroy();
  m_multiviewTileRasterPipeline.destroy();
  m_tileAovAtlas.destroy();
  m_atlasDirty = false;
  m_atlasExportValid = false;
}

void TileAtlasPass::destroyGraphicsPipeline()
{
  m_multiviewTileRasterPipeline.destroyGraphicsPipeline();
}

void TileAtlasPass::setConfig(TileAtlasConfig config)
{
  config.sanitize();
  if(config != m_config)
  {
    m_config = config;
    if(!m_config.enabled)
    {
      m_atlasDirty = false;
      m_atlasExportValid = false;
    }
  }
}

void TileAtlasPass::record(const VkCommandBuffer& cmdBuf,
                           const RasterGpuScene& scene,
                           RasterSceneDescriptors& descriptors,
                           RasterViewUniforms& viewUniforms,
                           const PreviewRasterPipeline& previewPipeline)
{
  m_atlasExportValid = false;
  const auto& cameras = viewUniforms.getCameras();
  if(!m_config.enabled || cameras.empty())
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
    if(!m_multiviewTileRasterPipeline.ensureResources(descriptors.layout(), previewPipeline, effectiveViewCount))
    {
      skip("failed to create multiview raster pipeline");
      return;
    }

    if(!m_tileAovAtlas.ensureResources(config, previewPipeline))
    {
      skip("failed to create tile atlas resources");
      return;
    }

    if(!m_multiviewTileTargets.ensureResources(config, previewPipeline, m_multiviewTileRasterPipeline.getRenderPass(),
                                               effectiveViewCount))
    {
      skip("failed to create layered tile resources");
      return;
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "[RasterRenderer] Tile multiview setup failed: " << e.what() << std::endl;
    logUnavailableOnce("setup failed");
    return;
  }

  for(uint32_t firstCameraIndex = 0; firstCameraIndex < cameraCount; firstCameraIndex += effectiveViewCount)
  {
    const uint32_t batchCameraCount = std::min(effectiveViewCount, cameraCount - firstCameraIndex);
    viewUniforms.updateTileFrameUniformBufferForBatch(cmdBuf, firstCameraIndex, batchCameraCount, effectiveViewCount,
                                                      tileExtent, scene.getLightCount());
    const bool rendered =
        m_multiviewTileRasterPipeline.rasterize(cmdBuf, m_multiviewTileTargets.getFramebuffer(), tileExtent,
                                                descriptors.set(), scene.getMeshBuffers(), scene.getInstances(),
                                                scene.getInstanceIds());
    const bool copied =
        rendered && m_tileAovAtlas.copyLayersFrom(cmdBuf, m_multiviewTileTargets, firstCameraIndex, batchCameraCount);
    if(!copied)
    {
      skip("batch render or layer copy failed");
      return;
    }
  }

  m_atlasDirty = cameraCount > 0;
  m_atlasExportValid = cameraCount > 0;
}

std::optional<ExportedRasterAovTexture> TileAtlasPass::getAovTexture(RasterAov aov) const
{
  if(!m_atlasExportValid || !m_tileAovAtlas.hasImages())
  {
    return std::nullopt;
  }
  return m_tileAovAtlas.getAovTexture(aov);
}

void TileAtlasPass::markConsumed(const std::string& /*outputDirectory*/)
{
  // Local tile file output is disabled; Hydra consumes tile atlas AOVs directly.
  m_atlasDirty = false;
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

void TileAtlasPass::logUnavailableOnce(const char* reason)
{
  if(m_multiviewUnsupportedLogged)
  {
    return;
  }

  m_multiviewUnsupportedLogged = true;
  std::cerr << "[RasterRenderer] Tile multiview unavailable";
  if(reason != nullptr && reason[0] != '\0')
  {
    std::cerr << ": " << reason;
  }
  std::cerr << std::endl;
}
