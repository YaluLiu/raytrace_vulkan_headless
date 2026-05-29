#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <raster/raster_session.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "core/raster_frame_executor.hpp"

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
}

RasterSession::RasterSession() {}

RasterSession::~RasterSession()
{
  cleanup();
}

void RasterSession::setup(int width, int height)
{
  m_width  = width;
  m_height = height;
  setupCamera();
  setupContext();
  setupRenderer();
}

void RasterSession::resize(int w, int h)
{
  if(w <= 0 || h <= 0)
  {
    return;
  }

  m_width  = w;
  m_height = h;
  CameraManip.setWindowSize(m_width, m_height);
  m_renderer.onResize(w, h);
}

void RasterSession::setupCamera()
{
  CameraManip.setWindowSize(m_width, m_height);
  CameraManip.setLookat(glm::vec3(5, 4, -4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
}

void RasterSession::setPluginSearchRoot(std::string pluginSearchRoot)
{
  m_pluginSearchRoot = std::move(pluginSearchRoot);
}

void RasterSession::setupContext()
{
  NVPSystem system("raster_session");

  defaultSearchPaths.clear();
  addSearchPathIfExists(defaultSearchPaths, fs::current_path() / "raster");
  addSearchPathIfExists(defaultSearchPaths, fs::path(NVPSystem::exePath()) / PROJECT_RELDIRECTORY);
  addSearchPathIfExists(defaultSearchPaths, fs::path(NVPSystem::exePath()) / PROJECT_RELDIRECTORY / "..");

  if(!m_pluginSearchRoot.empty())
  {
    addSearchPathIfExists(defaultSearchPaths, m_pluginSearchRoot);
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

  if(!m_vkctx.initInstance(contextInfo))
  {
    throw std::runtime_error("Failed to initialize Vulkan instance");
  }

  auto compatibleDevices = m_vkctx.getCompatibleDevices(contextInfo);
  if(compatibleDevices.empty())
  {
    throw std::runtime_error(
        "No compatible Vulkan device found for rasterization, ray tracing, and external memory extensions");
  }

  if(!m_vkctx.initDevice(compatibleDevices[0], contextInfo))
  {
    throw std::runtime_error("Failed to initialize Vulkan device");
  }
}

void RasterSession::setupRenderer()
{
  m_renderer.create(m_vkctx.m_instance, m_vkctx.m_device, m_vkctx.m_physicalDevice, m_vkctx.m_queueGCT.familyIndex,
                    {uint32_t(m_width), uint32_t(m_height)});
  m_rendererCreated = true;
}

void RasterSession::createRenderResources()
{
  m_renderer.createPreviewAovTargets();
  m_renderer.createLightBuffer();

  m_renderer.createSceneDescriptors();
  m_renderer.ensureViewUniformBuffers();
  m_renderer.createObjectDescriptionBuffer();
  m_renderer.updateSceneDescriptorBindings();
  m_renderer.createRayTracingResources();

  m_renderer.createPreviewOutputPipeline();
  m_resourcesCreated = true;
}

void RasterSession::render()
{
  m_renderer.ensureFrameUniformCapacity(m_renderer.getRequiredFrameUniformSlots());
  m_renderer.flushRayTracingUpdates();

  auto                   curFrame = m_renderer.getCurFrame();
  const VkCommandBuffer& cmdBuf   = m_renderer.getCommandBuffers()[curFrame];

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  raster::recordFramePasses(m_renderer, cmdBuf);

  vkEndCommandBuffer(cmdBuf);
  m_renderer.submitCurrentCommandBufferAndWait();
  try
  {
    m_renderer.markTileAovAtlasConsumed();
  }
  catch(const std::exception& e)
  {
    std::cerr << "[RasterSession] Failed to mark tile AOV atlas consumed: " << e.what() << std::endl;
  }
}

void RasterSession::saveFrame(std::string outputImagePath)
{
  m_renderer.saveOffscreenColorToFile(outputImagePath.c_str());
}

void RasterSession::cleanup()
{
  if(_cleaned)
  {
    return;
  }
  _cleaned = true;

  if(m_rendererCreated && m_renderer.getDevice() != VK_NULL_HANDLE)
  {
    vkDeviceWaitIdle(m_renderer.getDevice());
    if(m_resourcesCreated)
    {
      m_renderer.destroyResources();
    }
    m_renderer.destroy();
  }

  if(m_vkctx.m_device != VK_NULL_HANDLE || m_vkctx.m_instance != VK_NULL_HANDLE)
  {
    m_vkctx.deinit();
  }
}
