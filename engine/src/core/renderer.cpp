#include "core/renderer_internal.hpp"

#include "core/renderer_resources.hpp"

#include <memory>
#include <utility>

Renderer::Renderer() : m_impl(std::make_unique<Impl>()) {}

Renderer::~Renderer() = default;

Renderer::Renderer(Renderer&&) noexcept = default;

Renderer& Renderer::operator=(Renderer&&) noexcept = default;

void Renderer::create(VkInstance instance,
                            VkDevice device,
                            VkPhysicalDevice physicalDevice,
                            uint32_t queueFamily,
                            VkExtent2D size)
{
  setup(instance, device, physicalDevice, queueFamily);
  m_impl->createCommandBuffers();
  m_impl->sizeRef() = size;
}

void Renderer::destroy()
{
  m_impl->destroy();
}

VkDevice Renderer::getDevice() const
{
  return m_impl->getDevice();
}

const std::vector<VkCommandBuffer>& Renderer::getCommandBuffers() const
{
  return m_impl->getCommandBuffers();
}

uint32_t Renderer::getCurFrame() const
{
  return m_impl->getCurFrame();
}

void Renderer::submitCurrentCommandBufferAndWait()
{
  m_impl->submitCurrentCommandBufferAndWait();
}

void Renderer::setup(const VkInstance& instance,
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

const std::vector<CameraSpec>& Renderer::getCameras() const
{
  return m_viewUniforms.getCameras();
}

std::optional<ExportedAovTexture> Renderer::GetAovTexture(Aov aov) const
{
  return m_outputController.getAovTexture(aov);
}

void Renderer::configureOutputs(RendererOutputConfig config)
{
  m_outputController.applyConfig(std::move(config));
  engine::ensureFrameUniformCapacity(*this, engine::getRequiredFrameUniformSlots(*this));
}

LidarFramePointCloud Renderer::readLidarPointCloudFrame()
{
  return m_outputController.readLidarPointCloudFrame();
}

HeightScanFrame Renderer::readHeightScanFrame()
{
  return m_outputController.readHeightScanFrame();
}

void Renderer::setMainCameraClipRange(float clipStart, float clipEnd)
{
  m_viewUniforms.setMainCameraClipRange(clipStart, clipEnd);
}

float Renderer::getMainCameraClipStart() const
{
  return m_viewUniforms.getMainCameraClipStart();
}

float Renderer::getMainCameraClipEnd() const
{
  return m_viewUniforms.getMainCameraClipEnd();
}

void Renderer::setCameras(std::vector<CameraSpec> cameras)
{
  m_viewUniforms.setCameras(std::move(cameras));
  engine::ensureFrameUniformCapacity(*this, engine::getRequiredFrameUniformSlots(*this));
}

void Renderer::setMainCamera(const CameraSpec& camera)
{
  m_viewUniforms.setMainCamera(camera);
}

TileAtlasConfig Renderer::getTileConfig() const
{
  return m_outputController.getTileConfig();
}

bool Renderer::isTileMultiviewSupported() const
{
  return m_outputController.isTileMultiviewSupported();
}

uint32_t Renderer::getTileMultiviewMaxViewCount() const
{
  return m_outputController.getTileMultiviewMaxViewCount();
}

size_t Renderer::getInstanceCount() const
{
  return m_gpuScene.getInstanceCount();
}

InstanceInfo Renderer::getInstance(size_t index) const
{
  const auto& instance = m_gpuScene.getInstance(index);
  return InstanceInfo{
      instance.transform,
      instance.objIndex,
      0,
      instance.visible,
  };
}

size_t Renderer::getMeshSourceCount() const
{
  return m_gpuScene.getMeshSourceCount();
}

void Renderer::createRayTracingResources()
{
  m_gpuScene.createRayTracingResources();
}

void Renderer::destroyRayTracingResources()
{
  m_gpuScene.destroyRayTracingResources();
}

void Renderer::flushRayTracingUpdates()
{
  m_gpuScene.flushRayTracingUpdates();
}

bool Renderer::hasRayTracingTlas() const
{
  return m_gpuScene.hasRayTracingTlas();
}

VkAccelerationStructureKHR Renderer::getRayTracingTlas() const
{
  return m_gpuScene.getRayTracingTlas();
}

std::optional<TlasDescriptorInfo> Renderer::getRayTracingTlasDescriptorInfo() const
{
  return m_gpuScene.getRayTracingTlasDescriptorInfo();
}

void Renderer::onResize(int w, int h)
{
  engine::resizeRenderTargets(*this, w, h);
}
