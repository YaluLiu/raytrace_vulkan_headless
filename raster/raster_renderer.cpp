#include "raster_renderer.hpp"

#include <iostream>
#include <utility>

void RasterRenderer::setup(const VkInstance& instance,
                           const VkDevice& device,
                           const VkPhysicalDevice& physicalDevice,
                           uint32_t queueFamily)
{
  AppOffline::setup(instance, device, physicalDevice, queueFamily);
  m_alloc.init(device, physicalDevice);
  m_alloc.getStaging()->setFreeUnusedOnRelease(false);
  m_debug.setup(m_device);

  m_gpuScene.setup(m_device, queueFamily, m_alloc, m_debug);
  m_viewUniforms.setup(m_device, physicalDevice, m_alloc, m_debug);
  m_outputController.setup(m_device, physicalDevice, queueFamily, m_alloc, m_debug);
}

std::optional<ExportedRasterAovTexture> RasterRenderer::GetAovTexture(RasterAov aov) const
{
  return m_outputController.getAovTexture(aov);
}

void RasterRenderer::setMainCameraClipRange(float clipStart, float clipEnd)
{
  m_viewUniforms.setMainCameraClipRange(clipStart, clipEnd);
}

void RasterRenderer::setCameras(std::vector<RasterCameraSpec> cameras)
{
  m_viewUniforms.setCameras(std::move(cameras));
  ensureFrameUniformCapacity(getRequiredFrameUniformSlots());
}

void RasterRenderer::setMainCamera(const RasterCameraSpec& camera)
{
  m_viewUniforms.setMainCamera(camera);
}

void RasterRenderer::setTileConfig(TileAtlasConfig config)
{
  m_outputController.setTileConfig(config);
  ensureFrameUniformCapacity(getRequiredFrameUniformSlots());
}

void RasterRenderer::setRequestedTileAovChannels(TileAovChannelMask channels)
{
  m_outputController.setRequestedTileAovChannels(channels);
}

void RasterRenderer::updateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
  updateUniformBufferForExtent(cmdBuf, m_size);
}

void RasterRenderer::updateUniformBufferForExtent(const VkCommandBuffer& cmdBuf, VkExtent2D renderSize)
{
  m_viewUniforms.updateFrameUniformBuffer(cmdBuf, renderSize, m_gpuScene.getLightCount());
}

void RasterRenderer::createUniformBuffer()
{
  ensureFrameUniformCapacity(getRequiredFrameUniformSlots());
  ensureTileFrameUniformBuffer();
}

uint32_t RasterRenderer::getRequiredFrameUniformSlots() const
{
  return 1;
}

void RasterRenderer::ensureFrameUniformCapacity(uint32_t slotCount)
{
  if(m_viewUniforms.ensureFrameUniformCapacity(slotCount))
  {
    updateFrameUniformDescriptor();
  }
}

void RasterRenderer::updateFrameUniformDescriptor()
{
  m_sceneDescriptors.updateFrameUniform(m_viewUniforms.getFrameUniformDescriptorInfo());
}

void RasterRenderer::ensureTileFrameUniformBuffer()
{
  if(m_viewUniforms.ensureTileFrameUniformBuffer())
  {
    updateTileFrameUniformDescriptor();
  }
}

void RasterRenderer::updateTileFrameUniformDescriptor()
{
  m_sceneDescriptors.updateTileFrameUniform(m_viewUniforms.getTileFrameUniformDescriptorInfo());
}

void RasterRenderer::updateTileFrameUniformBufferForBatch(const VkCommandBuffer& cmdBuf,
                                                          uint32_t firstCameraIndex,
                                                          uint32_t cameraCount,
                                                          uint32_t viewCount,
                                                          VkExtent2D tileExtent)
{
  m_viewUniforms.updateTileFrameUniformBufferForBatch(cmdBuf, firstCameraIndex, cameraCount, viewCount, tileExtent,
                                                      m_gpuScene.getLightCount());
}

uint32_t RasterRenderer::getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const
{
  return m_outputController.getTileMultiviewEffectiveViewCount(cameraCount, capacity);
}

void RasterRenderer::logTileMultiviewUnavailableOnce(const char* reason)
{
  std::cerr << "[RasterRenderer] Tile multiview unavailable";
  if(reason != nullptr && reason[0] != '\0')
  {
    std::cerr << ": " << reason;
  }
  std::cerr << std::endl;
}

void RasterRenderer::createObjDescriptionBuffer()
{
  m_gpuScene.createObjDescriptionBuffer();
}

void RasterRenderer::destroyResources()
{
  m_outputController.destroy();
  m_sceneDescriptors.destroy();
  m_viewUniforms.destroy();
  m_gpuScene.destroy();
  m_alloc.deinit();
}

void RasterRenderer::onResize(int w, int h)
{
  if(w <= 0 || h <= 0)
  {
    return;
  }

  if(w == static_cast<int>(m_size.width) && h == static_cast<int>(m_size.height))
  {
    return;
  }

  m_size.width = w;
  m_size.height = h;
  createOffscreenRender();
}

void RasterRenderer::createOffscreenRender()
{
  m_outputController.createResources(m_size);
}
