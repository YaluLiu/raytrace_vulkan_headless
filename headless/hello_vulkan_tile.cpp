#include "hello_vulkan.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

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

void HelloVulkan::saveTileAtlasOutputs(const std::string & /*outputDirectory*/)
{
  // Local tile file output is disabled; Hydra consumes tile atlas AOVs directly.
  m_tileAtlasDirty = false;
}
