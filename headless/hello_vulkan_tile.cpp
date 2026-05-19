#include "hello_vulkan.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

void HelloVulkan::renderTileAtlasMultiview(const VkCommandBuffer &cmdBuf)
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

  const uint32_t cameraCount = static_cast<uint32_t>(std::min<size_t>(m_cameras.size(), capacity));
  const uint32_t effectiveViewCount = getTileMultiviewEffectiveViewCount(cameraCount, capacity);
  m_tileMultiviewEffectiveViewCount = effectiveViewCount;

  auto skip = [&](const char *reason)
  {
    logTileMultiviewUnavailableOnce(reason);
  };

  if(effectiveViewCount == 0)
  {
    skip("device unsupported or no usable views");
    return;
  }

  ensureTileFrameUniformBuffer();
  updateTileFrameUniformDescriptor();

  const VkExtent2D tileExtent = config.tileExtent();
  try
  {
    if(!m_tileMultiviewRasterPipeline.ensureResources(m_descSetLayout, m_mainRasterPipeline, effectiveViewCount))
    {
      skip("failed to create multiview raster pipeline");
      return;
    }

    if(!m_tileAtlas.ensureResources(config, m_mainRasterPipeline))
    {
      skip("failed to create tile atlas resources");
      return;
    }

    if(!m_tileLayerOutput.ensureResources(config, m_mainRasterPipeline, m_tileMultiviewRasterPipeline.getRenderPass(),
                                          effectiveViewCount))
    {
      skip("failed to create layered tile resources");
      return;
    }
  }
  catch(const std::exception &e)
  {
    std::cerr << "[HelloVulkan] Tile multiview setup failed: " << e.what() << std::endl;
    logTileMultiviewUnavailableOnce("setup failed");
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
      skip("batch render or layer copy failed");
      return;
    }
  }

  m_tileAtlasDirty = cameraCount > 0;
  m_tileAtlasExportValid = cameraCount > 0;
}

void HelloVulkan::saveTileAtlasOutputs(const std::string & /*outputDirectory*/)
{
  // Local tile file output is disabled; Hydra consumes tile atlas AOVs directly.
  m_tileAtlasDirty = false;
}
