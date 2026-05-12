#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include <cassert>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include "ray_trace_app.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "obj_loader.h"

std::vector<std::string> defaultSearchPaths;

namespace fs = std::filesystem;

namespace {
enum class HeadlessRenderPass
{
  UpdateUniforms,
  UpdateLights,
  UpdateInstanceIds,
  Rasterize,
};

constexpr std::array<HeadlessRenderPass, 4> kHeadlessRenderPassSequence{
    HeadlessRenderPass::UpdateUniforms,
    HeadlessRenderPass::UpdateLights,
    HeadlessRenderPass::UpdateInstanceIds,
    HeadlessRenderPass::Rasterize,
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

void executeHeadlessRenderPass(HelloVulkan& renderer, const VkCommandBuffer& cmdBuf, HeadlessRenderPass pass)
{
  switch(pass)
  {
    case HeadlessRenderPass::UpdateUniforms:
      renderer.updateUniformBuffer(cmdBuf);
      break;
    case HeadlessRenderPass::UpdateLights:
      renderer.updateLightBuffer(cmdBuf);
      break;
    case HeadlessRenderPass::UpdateInstanceIds:
      renderer.updateInstanceIdBuffer(cmdBuf);
      break;
    case HeadlessRenderPass::Rasterize:
      renderer.rasterize(cmdBuf);
      break;
  }
}
}

RayTraceApp::RayTraceApp() {}

RayTraceApp::~RayTraceApp()
{
  cleanup();
}

void RayTraceApp::setup(int width, int height)
{
  m_width  = width;
  m_height = height;
  setupCamera();
  setupContext();
  setupHelloVulkan();
}

void RayTraceApp::resize(int w, int h)
{
  m_helloVk.onResize(w, h);
}

void RayTraceApp::setupCamera()
{
  CameraManip.setWindowSize(m_width, m_height);
  CameraManip.setLookat(glm::vec3(5, 4, -4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
}

void RayTraceApp::UpdateCamera() {}

void RayTraceApp::setPluginSearchRoot(std::string pluginSearchRoot)
{
  m_pluginSearchRoot = std::move(pluginSearchRoot);
}

void RayTraceApp::setupContext()
{
  NVPSystem system("raytrace_vulkan_headless");

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

void RayTraceApp::setupHelloVulkan()
{
  nvvkhl::AppBaseVkCreateInfo createInfo;
  createInfo.instance       = m_vkctx.m_instance;
  createInfo.device         = m_vkctx.m_device;
  createInfo.physicalDevice = m_vkctx.m_physicalDevice;
  createInfo.queueIndices   = {m_vkctx.m_queueGCT.familyIndex};
  createInfo.size           = {uint32_t(m_width), uint32_t(m_height)};
  m_helloVk.create(createInfo);
  m_helloCreated = true;
}

void RayTraceApp::createRenderResources()
{
  m_helloVk.createOffscreenRender();
  m_helloVk.createLightBuffer();
  m_helloVk.createInstanceIdBuffer();

  m_helloVk.createDescriptorSetLayout();
  m_helloVk.createUniformBuffer();
  m_helloVk.createObjDescriptionBuffer();
  m_helloVk.updateDescriptorSet();

  m_helloVk.createRasterPipeline();
  m_resourcesCreated = true;
}

void RayTraceApp::render()
{
  auto                   curFrame = m_helloVk.getCurFrame();
  const VkCommandBuffer& cmdBuf   = m_helloVk.getCommandBuffers()[curFrame];

  VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  for(const HeadlessRenderPass pass : kHeadlessRenderPassSequence)
  {
    executeHeadlessRenderPass(m_helloVk, cmdBuf, pass);
  }

  vkEndCommandBuffer(cmdBuf);
  m_helloVk.submitFrame();
}

void RayTraceApp::saveFrame(std::string outputImagePath)
{
  m_helloVk.saveOffscreenColorToFile(outputImagePath.c_str());
}

void RayTraceApp::setRadarCamera(const RadarCameraInput& radarCamera)
{
  HelloVulkan::RadarCameraData radar{};
  radar.eye    = radarCamera.eye;
  radar.center = radarCamera.center;
  radar.up     = radarCamera.up;
  radar.fov    = radarCamera.fovDeg;
  m_helloVk.setRadarCamera(radar);
  m_helloVk.setRadarLidarParams(radarCamera.lidarParams);
}

void RayTraceApp::setHeightScanCamera(const SensorCameraInput& heightScanCamera)
{
  HelloVulkan::RadarCameraData camera{};
  camera.eye    = heightScanCamera.eye;
  camera.center = heightScanCamera.center;
  camera.up     = heightScanCamera.up;
  camera.fov    = heightScanCamera.fovDeg;
  m_helloVk.setHeightScanCamera(camera);
}

void RayTraceApp::setLidarEnabled(bool enabled)
{
  m_helloVk.setLidarEnabled(enabled);
}

bool RayTraceApp::isLidarEnabled() const
{
  return m_helloVk.isLidarEnabled();
}

void RayTraceApp::setHeightScanParams(const HeightScanParams& params)
{
  m_helloVk.setHeightScanParams(params);
}

void RayTraceApp::setHeightScanEnabled(bool enabled)
{
  m_helloVk.setHeightScanEnabled(enabled);
}

bool RayTraceApp::isHeightScanEnabled() const
{
  return m_helloVk.isHeightScanEnabled();
}

void RayTraceApp::cleanup()
{
  if(_cleaned)
  {
    return;
  }
  _cleaned = true;

  if(m_helloCreated && m_helloVk.getDevice() != VK_NULL_HANDLE)
  {
    vkDeviceWaitIdle(m_helloVk.getDevice());
    if(m_resourcesCreated)
    {
      m_helloVk.destroyResources();
    }
    m_helloVk.destroy();
  }

  if(m_vkctx.m_device != VK_NULL_HANDLE || m_vkctx.m_instance != VK_NULL_HANDLE)
  {
    m_vkctx.deinit();
  }
}

void RayTraceApp::loadScene()
{
  ObjLoader planeLoader;
  planeLoader.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true));
  m_helloVk.loadModel(planeLoader, glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

  ObjLoader wusonLoader;
  wusonLoader.loadModel(nvh::findFile("media/scenes/wuson.obj", defaultSearchPaths, true));
  m_helloVk.loadModel(wusonLoader);

  uint32_t  wusonId = 1;
  glm::mat4 identity{1};
  for(int i = 0; i < 5; i++)
  {
    m_helloVk.addInstance(identity, wusonId);
  }

  ObjLoader sphereLoader;
  sphereLoader.loadModel(nvh::findFile("media/scenes/sphere.obj", defaultSearchPaths, true));
  m_helloVk.loadModel(sphereLoader);

  m_startTime = std::chrono::system_clock::now();
}

void RayTraceApp::animation()
{
  std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
  m_helloVk.animationObject(diff.count());
  m_helloVk.animationInstances(diff.count());
}
