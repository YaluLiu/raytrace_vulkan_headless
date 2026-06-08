#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/commands_vk.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <engine/engine.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "core/frame_executor.hpp"
#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

std::vector<std::string> defaultSearchPaths;

namespace fs = std::filesystem;

namespace {
void addSearchPathIfExists(std::vector<std::string>& paths, const fs::path& path)
{
  if(path.empty() || !fs::exists(path))
  {
    return;
  }

  const std::string normalized = path.lexically_normal().string();
  if(std::find(paths.begin(), paths.end(), normalized) == paths.end())
  {
    paths.push_back(normalized);
  }
}

std::string getUsdRootFromEnvironment()
{
  const char* envValue = std::getenv("USD_ROOT");
  return envValue != nullptr ? std::string(envValue) : std::string();
}

void addExternalSharingExtensions(nvvk::ContextCreateInfo& contextInfo)
{
  contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);

  contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);

#ifdef _WIN32
  contextInfo.addDeviceExtension("VK_KHR_external_memory_win32");
  contextInfo.addDeviceExtension("VK_KHR_external_semaphore_win32");
  contextInfo.addDeviceExtension("VK_KHR_external_fence_win32");
#else
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#endif
}

void syncMainViewStateFromCameraManip(engine::EngineAccess::Impl& impl)
{
  impl.viewUniforms.setMainViewState(MainViewState{
      CameraManip.getMatrix(),
      CameraManip.getFov(),
      impl.viewUniforms.getMainCameraClipStart(),
      impl.viewUniforms.getMainCameraClipEnd(),
  });
}
}

void Engine::setup(int width, int height)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.width = width;
  impl.height = height;
  impl.cleaned = false;

  CameraManip.setWindowSize(impl.width, impl.height);
  CameraManip.setLookat(glm::vec3(5, 4, -4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

  setupContext();
  create(impl.vkctx.m_instance, impl.vkctx.m_device, impl.vkctx.m_physicalDevice, impl.vkctx.m_queueGCT.familyIndex,
         {uint32_t(impl.width), uint32_t(impl.height)});
  syncMainViewStateFromCameraManip(impl);
}

void Engine::setPluginSearchRoot(std::string pluginSearchRoot)
{
  engine::EngineAccess::impl(*this).pluginSearchRoot = std::move(pluginSearchRoot);
}

void Engine::resize(int width, int height)
{
  if(width <= 0 || height <= 0)
  {
    return;
  }

  auto& impl = engine::EngineAccess::impl(*this);
  impl.width = width;
  impl.height = height;
  CameraManip.setWindowSize(impl.width, impl.height);
  syncMainViewStateFromCameraManip(impl);
  onResize(width, height);
}

void Engine::setupContext()
{
  auto& impl = engine::EngineAccess::impl(*this);
  NVPSystem system("session");

  defaultSearchPaths.clear();
  addSearchPathIfExists(defaultSearchPaths, fs::current_path() / "engine");
  addSearchPathIfExists(defaultSearchPaths, fs::path(NVPSystem::exePath()) / PROJECT_RELDIRECTORY);
  addSearchPathIfExists(defaultSearchPaths, fs::path(NVPSystem::exePath()) / PROJECT_RELDIRECTORY / "..");

  if(!impl.pluginSearchRoot.empty())
  {
    addSearchPathIfExists(defaultSearchPaths, impl.pluginSearchRoot);
  }
  else
  {
    const std::string usdRoot = getUsdRootFromEnvironment();
    if(!usdRoot.empty())
    {
      addSearchPathIfExists(defaultSearchPaths, fs::path(usdRoot) / "plugin/usd/hdRobot");
    }
  }

  nvvk::ContextCreateInfo contextInfo;
  contextInfo.verboseUsed              = false;
  contextInfo.verboseCompatibleDevices = false;
  contextInfo.verboseAvailable         = false;
  contextInfo.setVersion(1, 2);

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);

  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeature);

  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

  addExternalSharingExtensions(contextInfo);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

  if(!impl.vkctx.initInstance(contextInfo))
  {
    throw std::runtime_error("Failed to initialize Vulkan instance");
  }
  impl.ownsContext = true;

  auto compatibleDevices = impl.vkctx.getCompatibleDevices(contextInfo);
  if(compatibleDevices.empty())
  {
    throw std::runtime_error(
        "No compatible Vulkan device found for graphics, ray tracing, and external memory extensions");
  }

  if(!impl.vkctx.initDevice(compatibleDevices[0], contextInfo))
  {
    throw std::runtime_error("Failed to initialize Vulkan device");
  }
}

void Engine::createRenderResources()
{
  engine::createRenderResources(*this);
  engine::EngineAccess::impl(*this).resourcesCreated = true;
}

void Engine::render()
{
  syncMainViewStateFromCameraManip(engine::EngineAccess::impl(*this));
  engine::prepareFrame(*this);

  auto                   curFrame = getCurFrame();
  const VkCommandBuffer& cmdBuf   = getCommandBuffers()[curFrame];

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  engine::recordFramePasses(*this, cmdBuf);

  vkEndCommandBuffer(cmdBuf);
  submitCurrentCommandBufferAndWait();
  try
  {
    engine::finishFrame(*this);
  }
  catch(const std::exception& e)
  {
    std::cerr << "[Engine] Failed to mark tile AOV atlas consumed: " << e.what() << std::endl;
  }
}

void Engine::saveFrame(std::string outputImagePath)
{
  saveOffscreenColorToFile(outputImagePath.c_str());
}

void Engine::cleanup()
{
  auto& impl = engine::EngineAccess::impl(*this);
  if(impl.cleaned)
  {
    return;
  }
  impl.cleaned = true;

  if(impl.engineCreated && getDevice() != VK_NULL_HANDLE)
  {
    vkDeviceWaitIdle(getDevice());
    if(impl.resourcesCreated)
    {
      engine::destroyRenderResources(*this);
      impl.resourcesCreated = false;
    }
    else
    {
      impl.alloc.deinit();
    }
    impl.nvvkhl::AppOffline::destroy();
    impl.engineCreated = false;
  }

  if(impl.ownsContext)
  {
    if(impl.vkctx.m_device != VK_NULL_HANDLE || impl.vkctx.m_instance != VK_NULL_HANDLE)
    {
      impl.vkctx.deinit();
    }
    impl.ownsContext = false;
  }
}
