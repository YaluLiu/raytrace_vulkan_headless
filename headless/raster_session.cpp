#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include <cassert>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "raster_session.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "obj_loader.h"

std::vector<std::string> defaultSearchPaths;

namespace fs = std::filesystem;

namespace {
enum class RasterFramePass
{
  UpdateUniforms,
  UpdateLights,
  RenderPreviewAovs,
};

constexpr std::array<RasterFramePass, 3> kRasterFramePassSequence{
    RasterFramePass::UpdateUniforms,
    RasterFramePass::UpdateLights,
    RasterFramePass::RenderPreviewAovs,
};

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

void addExternalMemoryExtensions(nvvk::ContextCreateInfo& contextInfo)
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

void executeRasterFramePass(RasterRenderer& renderer, const VkCommandBuffer& cmdBuf, RasterFramePass pass)
{
  switch(pass)
  {
    case RasterFramePass::UpdateUniforms:
      renderer.updateUniformBuffer(cmdBuf);
      break;
    case RasterFramePass::UpdateLights:
      renderer.updateLightBuffer(cmdBuf);
      break;
    case RasterFramePass::RenderPreviewAovs:
      renderer.renderTileAovAtlas(cmdBuf);
      renderer.renderPreviewAovs(cmdBuf);
      break;
  }
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

void RasterSession::UpdateCamera() {}

void RasterSession::setPluginSearchRoot(std::string pluginSearchRoot)
{
  m_pluginSearchRoot = std::move(pluginSearchRoot);
}

void RasterSession::setupContext()
{
  NVPSystem system("raster_session");

  defaultSearchPaths.clear();
  addSearchPathIfExists(defaultSearchPaths, fs::current_path() / "headless");
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

  addExternalMemoryExtensions(contextInfo);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

  if(!m_vkctx.initInstance(contextInfo))
  {
    throw std::runtime_error("Failed to initialize Vulkan instance");
  }

  auto compatibleDevices = m_vkctx.getCompatibleDevices(contextInfo);
  if(compatibleDevices.empty())
  {
    throw std::runtime_error("No compatible Vulkan device found for rasterization and external memory extensions");
  }

  if(!m_vkctx.initDevice(compatibleDevices[0], contextInfo))
  {
    throw std::runtime_error("Failed to initialize Vulkan device");
  }
}

void RasterSession::setupRenderer()
{
  nvvkhl::AppBaseVkCreateInfo createInfo;
  createInfo.instance       = m_vkctx.m_instance;
  createInfo.device         = m_vkctx.m_device;
  createInfo.physicalDevice = m_vkctx.m_physicalDevice;
  createInfo.queueIndices   = {m_vkctx.m_queueGCT.familyIndex};
  createInfo.size           = {uint32_t(m_width), uint32_t(m_height)};
  m_renderer.create(createInfo);
  m_rendererCreated = true;
}

void RasterSession::createRenderResources()
{
  m_renderer.createOffscreenRender();
  m_renderer.createLightBuffer();

  m_renderer.createDescriptorSetLayout();
  m_renderer.createUniformBuffer();
  m_renderer.createObjDescriptionBuffer();
  m_renderer.updateDescriptorSet();

  m_renderer.createPreviewRasterPipeline();
  m_resourcesCreated = true;
}

void RasterSession::render()
{
  m_renderer.ensureFrameUniformCapacity(m_renderer.getRequiredFrameUniformSlots());

  auto                   curFrame = m_renderer.getCurFrame();
  const VkCommandBuffer& cmdBuf   = m_renderer.getCommandBuffers()[curFrame];

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  for(const RasterFramePass pass : kRasterFramePassSequence)
  {
    executeRasterFramePass(m_renderer, cmdBuf, pass);
  }

  vkEndCommandBuffer(cmdBuf);
  m_renderer.submitFrame();
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
