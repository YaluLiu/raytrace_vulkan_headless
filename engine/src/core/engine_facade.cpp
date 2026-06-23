#include "core/renderer_internal.hpp"

#include "core/renderer_resources.hpp"
#include "nvh/cameramanipulator.hpp"
#include "scene/camera_projection.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
struct ResolvedMainCamera
{
  glm::vec3 position{0.0f};
  glm::vec3 target{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float verticalFovDegrees{45.0f};
};

ResolvedMainCamera resolveMainCameraForManipulator(const CameraSpec& camera)
{
  ResolvedMainCamera resolved;
  resolved.position = camera.position;

  glm::vec3 forward = camera.forward;
  if(glm::length(forward) < 1e-6f)
  {
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  }

  resolved.up = camera.up;
  if(glm::length(glm::cross(forward, resolved.up)) < 1e-6f)
  {
    resolved.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  resolved.target = resolved.position + forward;
  resolved.verticalFovDegrees = std::clamp(camera.verticalFovDegrees, 1.0f, 179.0f);
  return resolved;
}
} // namespace

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
  engine::configureEngineSearchPaths(*this);
  m_impl->setup(instance, device, physicalDevice, queueFamily);
  auto& impl = engine::EngineImplAccess::impl(*this);
  impl.alloc.init(device, physicalDevice);
  impl.alloc.getStaging()->setFreeUnusedOnRelease(false);
  impl.debug.setup(impl.device());

  impl.gpuScene.setup(impl.device(), queueFamily, impl.alloc, impl.debug);
  impl.viewUniforms.setup(impl.device(), physicalDevice, impl.alloc, impl.debug);
  impl.outputController.setup(impl.device(), physicalDevice, queueFamily, impl.alloc, impl.debug);
}

const std::vector<CameraSpec>& Engine::getCameras() const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.viewUniforms.getCameras();
}

std::optional<ExportedAovTexture> Engine::GetAovTexture(Aov aov) const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.outputController.getAovTexture(aov);
}

void Engine::configureOutputs(RendererOutputConfig config)
{
  auto& impl = engine::EngineImplAccess::impl(*this);
  impl.outputController.applyConfig(std::move(config));
  engine::ensureFrameUniformCapacity(*this, engine::getRequiredFrameUniformSlots(*this));
}

LidarFramePointCloud Engine::readLidarPointCloudFrame()
{
  auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.outputController.readLidarPointCloudFrame();
}

HeightScanFrame Engine::readHeightScanFrame()
{
  auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.outputController.readHeightScanFrame();
}

void Engine::setMainCameraClipRange(float clipStart, float clipEnd)
{
  auto& impl = engine::EngineImplAccess::impl(*this);
  impl.viewUniforms.setMainCameraClipRange(clipStart, clipEnd);
}

float Engine::getMainCameraClipStart() const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.viewUniforms.getMainCameraClipStart();
}

float Engine::getMainCameraClipEnd() const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.viewUniforms.getMainCameraClipEnd();
}

void Engine::setCameras(std::vector<CameraSpec> cameras)
{
  auto& impl = engine::EngineImplAccess::impl(*this);
  impl.viewUniforms.setCameras(std::move(cameras));
  engine::ensureFrameUniformCapacity(*this, engine::getRequiredFrameUniformSlots(*this));
}

void Engine::setMainCamera(const CameraSpec& camera)
{
  auto& impl = engine::EngineImplAccess::impl(*this);
  const auto resolved = resolveMainCameraForManipulator(camera);
  CameraSpec projectionCamera = camera;
  projectionCamera.verticalFovDegrees = resolved.verticalFovDegrees;
  const VkExtent2D renderSize = impl.sizeRef();
  const float effectiveVerticalFovDegrees = ComputeVerticalFovForRenderTarget(projectionCamera, renderSize);
  CameraManip.setCamera({resolved.position, resolved.target, resolved.up, effectiveVerticalFovDegrees});
  impl.viewUniforms.setMainCamera(camera);
}

TileAtlasConfig Engine::getTileConfig() const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.outputController.getTileConfig();
}

bool Engine::isTileMultiviewSupported() const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.outputController.isTileMultiviewSupported();
}

uint32_t Engine::getTileMultiviewMaxViewCount() const
{
  const auto& impl = engine::EngineImplAccess::impl(*this);
  return impl.outputController.getTileMultiviewMaxViewCount();
}

void Engine::onResize(int width, int height)
{
  engine::resizeRenderTargets(*this, width, height);
}
