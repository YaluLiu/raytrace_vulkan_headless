#include "core/renderer_internal.hpp"

#include "core/renderer_resources.hpp"

#include <memory>
#include <utility>

Engine::Engine() : m_impl(std::make_unique<Impl>()) {}

Engine::~Engine()
{
  cleanup();
}

void Engine::create(VkInstance instance,
                            VkDevice device,
                            VkPhysicalDevice physicalDevice,
                            uint32_t queueFamily,
                            VkExtent2D size)
{
  setup(instance, device, physicalDevice, queueFamily);
  m_impl->createCommandBuffers();
  m_impl->sizeRef() = size;
  m_impl->engineCreated = true;
  m_impl->cleaned = false;
}

void Engine::destroy()
{
  cleanup();
}

VkDevice Engine::getDevice() const
{
  return m_impl->getDevice();
}

const std::vector<VkCommandBuffer>& Engine::getCommandBuffers() const
{
  return m_impl->getCommandBuffers();
}

uint32_t Engine::getCurFrame() const
{
  return m_impl->getCurFrame();
}

void Engine::submitCurrentCommandBufferAndWait()
{
  m_impl->submitCurrentCommandBufferAndWait();
}

void Engine::setup(const VkInstance& instance,
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

const std::vector<CameraSpec>& Engine::getCameras() const
{
  return m_viewUniforms.getCameras();
}

std::optional<ExportedAovTexture> Engine::GetAovTexture(Aov aov) const
{
  return m_outputController.getAovTexture(aov);
}

void Engine::configureOutputs(RendererOutputConfig config)
{
  m_outputController.applyConfig(std::move(config));
  engine::ensureFrameUniformCapacity(*this, engine::getRequiredFrameUniformSlots(*this));
}

LidarFramePointCloud Engine::readLidarPointCloudFrame()
{
  return m_outputController.readLidarPointCloudFrame();
}

HeightScanFrame Engine::readHeightScanFrame()
{
  return m_outputController.readHeightScanFrame();
}

void Engine::setMainCameraClipRange(float clipStart, float clipEnd)
{
  m_viewUniforms.setMainCameraClipRange(clipStart, clipEnd);
}

float Engine::getMainCameraClipStart() const
{
  return m_viewUniforms.getMainCameraClipStart();
}

float Engine::getMainCameraClipEnd() const
{
  return m_viewUniforms.getMainCameraClipEnd();
}

void Engine::setCameras(std::vector<CameraSpec> cameras)
{
  m_viewUniforms.setCameras(std::move(cameras));
  engine::ensureFrameUniformCapacity(*this, engine::getRequiredFrameUniformSlots(*this));
}

void Engine::setMainCamera(const CameraSpec& camera)
{
  m_viewUniforms.setMainCamera(camera);
}

TileAtlasConfig Engine::getTileConfig() const
{
  return m_outputController.getTileConfig();
}

bool Engine::isTileMultiviewSupported() const
{
  return m_outputController.isTileMultiviewSupported();
}

uint32_t Engine::getTileMultiviewMaxViewCount() const
{
  return m_outputController.getTileMultiviewMaxViewCount();
}

void Engine::onResize(int w, int h)
{
  engine::resizeRenderTargets(*this, w, h);
}
