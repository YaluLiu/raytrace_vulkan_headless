#include "core/raster_renderer_internal.hpp"

#include <iostream>
#include <memory>
#include <utility>

RasterRenderer::RasterRenderer() : m_impl(std::make_unique<Impl>()) {}

RasterRenderer::~RasterRenderer() = default;

RasterRenderer::RasterRenderer(RasterRenderer&&) noexcept = default;

RasterRenderer& RasterRenderer::operator=(RasterRenderer&&) noexcept = default;

void RasterRenderer::create(VkInstance instance,
                            VkDevice device,
                            VkPhysicalDevice physicalDevice,
                            uint32_t queueFamily,
                            VkExtent2D size)
{
  setup(instance, device, physicalDevice, queueFamily);
  m_impl->createCommandBuffers();
  m_impl->sizeRef() = size;
}

void RasterRenderer::destroy()
{
  m_impl->destroy();
}

VkDevice RasterRenderer::getDevice() const
{
  return m_impl->getDevice();
}

const std::vector<VkCommandBuffer>& RasterRenderer::getCommandBuffers() const
{
  return m_impl->getCommandBuffers();
}

uint32_t RasterRenderer::getCurFrame() const
{
  return m_impl->getCurFrame();
}

void RasterRenderer::submitCurrentCommandBufferAndWait()
{
  m_impl->submitCurrentCommandBufferAndWait();
}

void RasterRenderer::setup(const VkInstance& instance,
                           const VkDevice& device,
                           const VkPhysicalDevice& physicalDevice,
                           uint32_t queueFamily)
{
  m_impl->setup(instance, device, physicalDevice, queueFamily);
  m_alloc.init(device, physicalDevice);
  m_alloc.getStaging()->setFreeUnusedOnRelease(false);
  m_debug.setup(m_impl->device());

  m_gpuScene.setup(m_impl->device(), queueFamily, m_alloc, m_debug);
  m_viewUniforms.setup(m_impl->device(), physicalDevice, m_alloc, m_debug);
  m_outputController.setup(m_impl->device(), physicalDevice, queueFamily, m_alloc, m_debug);
}

const std::vector<RasterCameraSpec>& RasterRenderer::getCameras() const
{
  return m_viewUniforms.getCameras();
}

std::optional<ExportedRasterAovTexture> RasterRenderer::GetAovTexture(RasterAov aov) const
{
  return m_outputController.getAovTexture(aov);
}

void RasterRenderer::setMainCameraClipRange(float clipStart, float clipEnd)
{
  m_viewUniforms.setMainCameraClipRange(clipStart, clipEnd);
}

float RasterRenderer::getMainCameraClipStart() const
{
  return m_viewUniforms.getMainCameraClipStart();
}

float RasterRenderer::getMainCameraClipEnd() const
{
  return m_viewUniforms.getMainCameraClipEnd();
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

TileAtlasConfig RasterRenderer::getTileConfig() const
{
  return m_outputController.getTileConfig();
}

bool RasterRenderer::isTileMultiviewSupported() const
{
  return m_outputController.isTileMultiviewSupported();
}

uint32_t RasterRenderer::getTileMultiviewMaxViewCount() const
{
  return m_outputController.getTileMultiviewMaxViewCount();
}

size_t RasterRenderer::getInstanceCount() const
{
  return m_gpuScene.getInstanceCount();
}

RasterInstanceInfo RasterRenderer::getInstance(size_t index) const
{
  const auto& instance = m_gpuScene.getInstance(index);
  return RasterInstanceInfo{
      instance.transform,
      instance.objIndex,
      0,
      instance.visible,
  };
}

ModelLoader& RasterRenderer::getMutableMeshSourceLoader(size_t meshId)
{
  return m_gpuScene.getMutableMeshSourceLoader(meshId);
}

size_t RasterRenderer::getMeshSourceCount() const
{
  return m_gpuScene.getMeshSourceCount();
}

void RasterRenderer::recordFrameUniformUpdate(const VkCommandBuffer& cmdBuf)
{
  recordFrameUniformUpdateForExtent(cmdBuf, m_impl->sizeRef());
}

void RasterRenderer::recordFrameUniformUpdateForExtent(const VkCommandBuffer& cmdBuf, VkExtent2D renderSize)
{
  m_viewUniforms.updateFrameUniformBuffer(cmdBuf, renderSize, m_gpuScene.getLightCount());
}

void RasterRenderer::ensureViewUniformBuffers()
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

void RasterRenderer::createObjectDescriptionBuffer()
{
  m_gpuScene.createObjectDescriptionBuffer();
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

  if(w == static_cast<int>(m_impl->sizeRef().width) && h == static_cast<int>(m_impl->sizeRef().height))
  {
    return;
  }

  m_impl->sizeRef().width = w;
  m_impl->sizeRef().height = h;
  createPreviewAovTargets();
}

void RasterRenderer::createPreviewAovTargets()
{
  m_outputController.createPreviewTargets(m_impl->sizeRef());
}
