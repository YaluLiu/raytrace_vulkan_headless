#include "core/raster_renderer_internal.hpp"

#include "core/raster_renderer_resources.hpp"

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

void RasterRenderer::setLidarSensors(std::vector<RasterLidarSensorSpec> sensors)
{
  m_outputController.setLidarSensors(std::move(sensors));
}

void RasterRenderer::setLidarVisualizationConfig(RasterLidarVisualizationConfig config)
{
  m_outputController.setLidarVisualizationConfig(config);
}

RasterLidarFramePointCloud RasterRenderer::readLidarPointCloudFrame()
{
  return m_outputController.readLidarPointCloudFrame();
}

void RasterRenderer::setHeightScanSensors(std::vector<RasterHeightScanSensorSpec> sensors)
{
  m_outputController.setHeightScanSensors(std::move(sensors));
}

void RasterRenderer::setHeightScanVisualizationConfig(RasterHeightScanVisualizationConfig config)
{
  m_outputController.setHeightScanVisualizationConfig(config);
}

RasterHeightScanFrame RasterRenderer::readHeightScanFrame()
{
  return m_outputController.readHeightScanFrame();
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
  raster::ensureFrameUniformCapacity(*this, raster::getRequiredFrameUniformSlots(*this));
}

void RasterRenderer::setMainCamera(const RasterCameraSpec& camera)
{
  m_viewUniforms.setMainCamera(camera);
}

void RasterRenderer::setTileConfig(TileAtlasConfig config)
{
  m_outputController.setTileConfig(config);
  raster::ensureFrameUniformCapacity(*this, raster::getRequiredFrameUniformSlots(*this));
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

size_t RasterRenderer::getMeshSourceCount() const
{
  return m_gpuScene.getMeshSourceCount();
}

void RasterRenderer::createRayTracingResources()
{
  m_gpuScene.createRayTracingResources();
}

void RasterRenderer::destroyRayTracingResources()
{
  m_gpuScene.destroyRayTracingResources();
}

void RasterRenderer::flushRayTracingUpdates()
{
  m_gpuScene.flushRayTracingUpdates();
}

bool RasterRenderer::hasRayTracingTlas() const
{
  return m_gpuScene.hasRayTracingTlas();
}

VkAccelerationStructureKHR RasterRenderer::getRayTracingTlas() const
{
  return m_gpuScene.getRayTracingTlas();
}

std::optional<RasterTlasDescriptorInfo> RasterRenderer::getRayTracingTlasDescriptorInfo() const
{
  return m_gpuScene.getRayTracingTlasDescriptorInfo();
}

void RasterRenderer::onResize(int w, int h)
{
  raster::resizeRenderTargets(*this, w, h);
}
